#include "Cache.h"
#include "CacheConstants.h"
#include "CacheController.h"	
#include "CacheSet.h"
#include "vector"
#include "CacheJob.h"
#include "queue"
#include "BusRequest.h"
#include "CacheLine.h"

/*
Manages generating busrequests, handles processing of all the processor's requests,
and maintains its own LRU cache
*/
Cache::Cache(int pId, CacheConstants consts, std::queue<CacheJob*>* jobQueue)
{
	cacheConstants = consts;
	//make a vector of the CacheSet 
	localCache.resize(cacheConstants.getNumSets());
	for(int i = 0; i < cacheConstants.getNumSets(); i++){
		localCache[i] = new CacheSet(&consts);
	}
	processorId = pId;
	pendingJobs = *jobQueue;
	currentJob = NULL;
	busRequest = NULL;
	haveBusRequest = false;
	busy = false;
	startServiceCycle = 0;
	jobCycleCost = 0;

}

void Cache::setPId(int pid){
	processorId = pid;
}


/*
Given an address, and two int*, set the pointer values to the set number and 
the tag for the address
*/
void Cache::decode_address(unsigned long long address, int* whichSet, int* tag)
{
	int numSetBits = cacheConstants.getNumSetBits();
	int numBytesBits = cacheConstants.getNumBytesBits();
	int numTagBits = cacheConstants.getNumAddressBits() - (numSetBits + numBytesBits);

	int currSet = (address >> numBytesBits) & ((1 << numSetBits)-1);
	int currTag = (address >> (numSetBits + numBytesBits)) & ((1 << numTagBits)-1);

	*whichSet = currSet;
	*tag = currTag;

}

unsigned long long Cache::getTotalMemoryCost(int set, int tag)
{
	unsigned long long result = cacheConstants.getMemoryResponseCycleCost();
	CacheSet* currSet = localCache[set]; 
	if (!(*currSet).hasLine(tag))
	{
		if ((*currSet).isFull())
		{
			result = result*2;
		}
	}
	return result;
}

/*
For a given state, see if the line the current job we are working on is in that state
*/
bool Cache::lineInState(CacheLine::State state){
	int set = 0;
	int tag = 0;
	decode_address((*currentJob).getAddress(), &set, &tag);
	for(int i = 0; i < localCache.size(); i++){
		if((localCache[i] != NULL) && (*localCache[i]).hasLine(tag)){
			CacheLine* theLine = (*localCache[i]).getLine(tag);
			if((*theLine).getState() == state){
				return true;
			}
		}
	}
	return false;
}


/*
process a cache request, and ask for bus usage if necessary
*/
void Cache::handleRequest(){
	if (!busy){
		//so there are still jobs and we're not doing one right now
		if(!pendingJobs.empty()){
			currentJob = pendingJobs.front();
			pendingJobs.pop();
			printf("lets make a job for cache %d \n", processorId);
			if((*currentJob).isWrite()){
				//so if in the MSI protocol
				if(cacheConstants.getProtocol() == CacheConstants::MSI){
					if(lineInState(CacheLine::modified)){
						//so we can service this request ez
						startServiceCycle = cacheConstants.getCycle();
						jobCycleCost = cacheConstants.getCacheHitCycleCost();
						busy = true;
						haveBusRequest = false;
					}
					else{
						haveBusRequest = true;
						busy = true;
						int set = 0;
						int tag = 0;
						decode_address((*currentJob).getAddress(), &set, &tag);
						//the cycle cost can be changed for different protocols and such
						unsigned long long memoryCost = getTotalMemoryCost(set, tag);

						busRequest = new BusRequest(BusRequest::BusRdX, set, tag,
							memoryCost);
						jobCycleCost = cacheConstants.getMemoryResponseCycleCost();
					}
				}
			}
			if((*currentJob).isRead()){
				if(cacheConstants.getProtocol() == CacheConstants::MSI){
					if(lineInState(CacheLine::modified) || lineInState(CacheLine::shared)){
						//cache hit
						haveBusRequest = false;
						busy = true;
						startServiceCycle = cacheConstants.getCycle();
						jobCycleCost = cacheConstants.getCacheHitCycleCost();
					}
					else{
						//so we need to issue a request for the line
						haveBusRequest = true;
						busy = true;
						int set = 0;
						int tag = 0;
						decode_address((*currentJob).getAddress(), &set, &tag);
						busRequest = new BusRequest(BusRequest::BusRd, set, tag,
							cacheConstants.getMemoryResponseCycleCost());
						jobCycleCost = cacheConstants.getMemoryResponseCycleCost();
					}
				}
			}
		}
	}
}

//return True if we have an outstanding bus request to issue, false otherwise
bool Cache::hasBusRequest(){
	return haveBusRequest;
}

//return the current busRequest 
BusRequest* Cache::getBusRequest(){
	printf("cache %d got able to put out a bus request \n", processorId);
	startServiceCycle = cacheConstants.getCycle();
	return busRequest;
}

/*
Read the current BusRequest that another cache issued to the bus
and parse it to see if you need to update our own local cache
*/
Cache::SnoopResult Cache::snoopBusRequest(BusRequest* request){

	SnoopResult result = NONE;
	CacheSet* tempSet = localCache[(*request).getSet()];
	if((*tempSet).hasLine((*request).getTag())){
		//so we do have this line
		CacheLine* tempLine = (*tempSet).getLine((*request).getTag());
		if(cacheConstants.getProtocol() == CacheConstants::MSI){
			//so in the MSI protocol
			if((*request).getCommand() == BusRequest::BusRd){
				//so a bus read
				if((*tempLine).getState() == CacheLine::shared){
					//could have a cache respond with the data needed
					//but we just let main memory handle it
					result = Cache::SHARED;
					return result;
				}//shared
				if((*tempLine).getState() == CacheLine::modified){
					//FLUSH LINE TO MEMORY
					//and set the state to Shared
					result = FLUSH;
					(*tempLine).setState(CacheLine::shared);
					return result;
				}//modified
				if((*tempLine).getState() == CacheLine::invalid){
					//We shouldn't do anything here – the line isn't even in the cache
					return result;
				}
			}//busrd
			if ((*request).getCommand() == BusRequest::BusRdX)
			{
				if((*tempLine).getState() == CacheLine::shared){
					//invalidate our line
					(*tempLine).setState(CacheLine::invalid);
					return result;
				}//shared
				if((*tempLine).getState() == CacheLine::modified){
					//FLUSH LINE TO MEMORY
					//and set the state to Shared
					result = FLUSH;
					(*tempLine).setState(CacheLine::invalid);
					return result;
				}//modified
				if((*tempLine).getState() == CacheLine::invalid){
					//We shouldn't do anything here – the line isn't even in the cache
					return result;
				}	
			}
		}//msi protocol
	}//we have the line
	return result;
}



/*
Update to store the new line requested
*/
void Cache::busJobDone(){
	printf("cache %d has just been told it has finished a job \n", processorId);
	unsigned long long jobAddr = (*currentJob).getAddress();
	int currJobSet = 0;
	int currJobTag = 0;
	decode_address(jobAddr, &currJobSet, &currJobTag);

	haveBusRequest = false;
	busy = false;
	CacheSet* currSet = localCache[currJobSet];

	//Need to tell if we need to evict a line from the set
	bool needToEvict = (*currSet).isFull() && (*currSet).hasLine(currJobTag);
	if (needToEvict)
	{
		//Evict the line
		(*currSet).evictLRULine();
	}

	if (!(*currSet).hasLine(currJobTag))
	{
		CacheLine* newLine = new CacheLine(jobAddr, currJobSet, currJobTag);
		(*currSet).addLine(newLine);
	}

	CacheLine* currLine = (*currSet).getLine(currJobTag); 
	(*currLine).lastUsedCycle = cacheConstants.getCycle();
	if((*currentJob).isWrite()){
		//Set the line's state to Modified
		(*currLine).setState(CacheLine::modified);
	}
	if((*currentJob).isRead()){
		//Set the line's state to Shared
		(*currLine).setState(CacheLine::shared);
	}
}



int Cache::getProcessorId(){
	return processorId;
}


void Cache::tick(){
	if(startServiceCycle + jobCycleCost <= cacheConstants.getCycle()){
		//finished a job
		busy = false;
	}

	if(!busy && pendingJobs.size() != 0){
		//so we're free to do a new request
		handleRequest();
	}
}

Cache::~Cache(void){

}
