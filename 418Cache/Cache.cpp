#include "Cache.h"
#include "CacheConstants.h"
#include "CacheController.h"	
#include "CacheSet.h"
#include "vector"
#include "CacheJob.h"
#include "queue"
#include "BusRequest.h"
#include "CacheLine.h"
#include "BusResponse.h"

/*
Manages generating busrequests, handles processing of all the processor's requests,
and maintains its own LRU cache
*/
Cache::Cache(int pId, CacheConstants consts, std::queue<CacheJob*>* jobQueue, CacheStats* st)
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
	busRequestBeingServiced = false;
	startServiceCycle = 0;
	jobCycleCost = 0;
	stats = st;
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
		//only pay the cycle cost if we have to evict a line with data we care about
		if ((*currSet).isFull() && (*currSet).evictLineModified())
		{
			result = result*2;
			(*stats).numFlush++;
			printf("flush from an evict modify \n");
		}
	}
	return result;
}

unsigned long long Cache::getGuaranteeTime(){
	return cacheConstants.getCycle() - 2 * cacheConstants.getPropagationDelayRectDiagonal();
}

/*
For a given state, see if the line the current job we are working on is in that state
*/
bool Cache::lineInState(CacheLine::State state){
	int set = 0;
	int tag = 0;
	decode_address((*currentJob).getAddress(), &set, &tag);
	for(unsigned int i = 0; i < localCache.size(); i++){
		if((localCache[i] != NULL) && (*localCache[i]).hasLine(tag)){
			CacheLine* theLine = (*localCache[i]).getLine(tag);
			if((*theLine).getState() == state){
				return true;
			}
		}
	}
	return false;
}

void Cache::setLineState(CacheLine::State state){
	int set = 0;
	int tag = 0;
	decode_address((*currentJob).getAddress(), &set, &tag);
	for(unsigned int i = 0; i < localCache.size(); i++){
		if((localCache[i] != NULL) && (*localCache[i]).hasLine(tag)){
			CacheLine* theLine = (*localCache[i]).getLine(tag);
			(*theLine).setState(state);
		}
	}
}

bool Cache::handleWriteModified(){
	if(lineInState(CacheLine::modified)){
		//so we can service this request ez
		startServiceCycle = cacheConstants.getCycle();
		jobCycleCost = cacheConstants.getCacheHitCycleCost();
		busy = true;
		haveBusRequest = false;
		(*stats).numHit++;
		printf("cache %d just got a cache HIT on a PrWr request for address %llx in MODIFIED state at cycle %llu \n", 			processorId, (*currentJob).getAddress(), cacheConstants.getCycle());
		return true;
	}
	return false;
}

bool Cache::handleWriteExclusive(){
	if(lineInState(CacheLine::exclusive)){
		//so have to change the line state to modified
		//but it's a hit so no need for bus
		startServiceCycle = cacheConstants.getCycle();
		jobCycleCost = cacheConstants.getCacheHitCycleCost();
		busy = true;
		haveBusRequest = false;
		(*stats).numHit++;
		(*stats).numExclusiveToModifiedTransitions++;
		//set it to modified state
		setLineState(CacheLine::modified);
		printf("cache %d just got a cache HIT on a PrWr request for address %llx in EXCLUSIVE state, so changed to MODIFIED state at cycle %llu\n",			processorId, (*currentJob).getAddress(), cacheConstants.getCycle());
		return true;
	}
	return false;
}

void Cache::handleWriteSharedInvalid(){
	haveBusRequest = true;
	busy = true;
	int set = 0;
	int tag = 0;
	decode_address((*currentJob).getAddress(), &set, &tag);
	//the cycle cost can be changed for different protocols and such
	unsigned long long memoryCost = getTotalMemoryCost(set, tag);
	(*stats).numMiss++;
	busRequest = new BusRequest(BusRequest::BusRdX, set, tag,
		memoryCost, (*currentJob).getAddress(), cacheConstants.getCycle(), processorId);
	jobCycleCost = memoryCost;
	printf("cache %d just got a cache MISS(or was SHARED) on a PrWr request for address %llx at cycle %llu \n", processorId, (*currentJob).getAddress(), cacheConstants.getCycle());

}

void Cache::handleWriteRequestMESI(){
	//only difference here is that we have to case if we're in exclusive state

	if(handleWriteModified()){
		return;
	}
	else if(handleWriteExclusive()){
		return;
	}else{
		handleWriteSharedInvalid();
	}
}


void Cache::handleWriteRequestMSI(){
	if(handleWriteModified()){
		return;
	}
	else{
		handleWriteSharedInvalid();
	}
}

bool Cache::handleReadHit(){
	if(lineInState(CacheLine::modified) || lineInState(CacheLine::shared) || lineInState(CacheLine::exclusive) || lineInState(CacheLine::owned)){
		//cache hit
		haveBusRequest = false;
		busy = true;
		startServiceCycle = cacheConstants.getCycle();
		jobCycleCost = cacheConstants.getCacheHitCycleCost();
		(*stats).numHit++;
		printf("cache %d just got a cache HIT on a PrRd request for address %llx at cycle %llu \n",			processorId, (*currentJob).getAddress(), cacheConstants.getCycle());
		return true;
	}
	return false;
}

void Cache::handleReadMiss(){
	//so we need to issue a request for the line
	haveBusRequest = true;
	busy = true;
	int set = 0;
	int tag = 0;
	decode_address((*currentJob).getAddress(), &set, &tag);
	busRequest = new BusRequest(BusRequest::BusRd, set, tag,
		cacheConstants.getMemoryResponseCycleCost(), (*currentJob).getAddress(), cacheConstants.getCycle(), processorId);
	jobCycleCost = cacheConstants.getMemoryResponseCycleCost();
	(*stats).numMiss++;
	printf("cache %d just got a cache MISS on a PrRd request for address %llx at cycle %llu \n",		processorId, (*currentJob).getAddress(), cacheConstants.getCycle());
}

void Cache::handleReadRequestMESI(){
	if(handleReadHit()){
		return;
	}
	else{
		handleReadMiss();
	}
}

void Cache::handleReadRequestMSI(){
	if(handleReadHit()){
		return;
	}
	else{
		handleReadMiss();
	}
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
			printf("lets make a job for cache %d at cycle %llu \n", processorId, cacheConstants.getCycle());
			if((*currentJob).isWrite()){
				if(cacheConstants.getProtocol() == CacheConstants::MSI){
					handleWriteRequestMSI();
					return;
				}
				if(cacheConstants.getProtocol() == CacheConstants::MESI){
					handleWriteRequestMESI();
					return;
				}
				if(cacheConstants.getProtocol() == CacheConstants::MOESI){
					if(lineInState(CacheLine::invalid) || lineInState(CacheLine::shared)){
						handleWriteSharedInvalid();
						return;
					}
					else if(lineInState(CacheLine::exclusive)){
						handleWriteExclusive();
						return;
					}
					else if(lineInState(CacheLine::modified)){
						handleWriteModified();
						return;
					}
					else if(lineInState(CacheLine::owned)){
						haveBusRequest = true;
						busy = true;
						int set = 0;
						int tag = 0;
						decode_address((*currentJob).getAddress(), &set, &tag);
						//its a hit
						unsigned long long memoryCost = cacheConstants.getCacheHitCycleCost();
						(*stats).numHit++;
						busRequest = new BusRequest(BusRequest::BusRdX, set, tag,
							memoryCost, (*currentJob).getAddress(), cacheConstants.getCycle(), processorId);
						jobCycleCost = cacheConstants.getCacheHitCycleCost();
						setLineState(CacheLine::modified);
						return;
					}
					else{
						handleWriteSharedInvalid();
						return;
					}
				}
			}
			if((*currentJob).isRead()){
				if(cacheConstants.getProtocol() == CacheConstants::MSI){
					handleReadRequestMSI();
				}
				if(cacheConstants.getProtocol() == CacheConstants::MESI)
				{
					handleReadRequestMESI();					
				}
				if(cacheConstants.getProtocol() == CacheConstants::MOESI){
					if(handleReadHit()){
						return;
					}
					else{
						handleReadMiss();
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
	printf("cache %d got able to put out a bus request for address %llx at cycle %llu \n", 		processorId, (*currentJob).getAddress(), cacheConstants.getCycle());
	startServiceCycle = cacheConstants.getCycle();
	busRequestBeingServiced = true;
	return busRequest;
}

BusResponse::SnoopResult Cache::handleBusRdShared(BusRequest* request, int setNum, int tagNum, CacheLine* tempLine){
	printf("cache number %d just changed set %d and tag %d at address %llx to from shared to shared from a read at cycle %llu \n", 		processorId, setNum, tagNum, (*request).address, cacheConstants.getCycle());
	return BusResponse::SHARED;
}

BusResponse::SnoopResult Cache::handleBusRdModified(BusRequest* request, int setNum, int tagNum, CacheLine* tempLine){
	//FLUSH LINE TO MEMORY and set the state to Shared
	(*stats).numFlush++;
	printf("cache %d just FLUSH set %d and tag %d for address %llx \n", processorId, setNum, tagNum, (*request).address);
	(*tempLine).setState(CacheLine::shared);
	printf("cache number %d just changed set %d and tag %d for address %llx from modified to shared from a read at cycle %llu \n",		processorId, setNum, tagNum, (*request).address, cacheConstants.getCycle());
	return 	BusResponse::FLUSH_MODIFIED_TO_SHARED;
}

BusResponse::SnoopResult Cache::handleBusRdInvalid(BusRequest* request, int setNum, int tagNum, CacheLine* tempLine){
	printf("cache number %d does not have set %d and tag %d for address %llx in its cache, ignoring snoop at cycle %llu \n",		processorId, setNum, tagNum, (*request).address, cacheConstants.getCycle());
	return BusResponse::NONE;
}

BusResponse::SnoopResult Cache::handleBusRdMESI(BusRequest* request, int setNum, int tagNum, CacheLine* tempLine){
	if((*tempLine).getState() == CacheLine::shared){
		return handleBusRdShared(request, setNum, tagNum, tempLine);
	}
	if((*tempLine).getState() == CacheLine::modified){
		return handleBusRdModified(request, setNum, tagNum, tempLine);
	}
	if((*tempLine).getState() == CacheLine::exclusive){
		//so no need to flush, but now we're not exclusive
		(*tempLine).setState(CacheLine::shared);
		printf("cache number %d just changed set %d and tag %d for address %llx from exclusive to shared from a read at cycle %llu \n",			processorId, setNum, tagNum, (*request).address, cacheConstants.getCycle());
		return BusResponse::SHARED;
	}
	else{
		return handleBusRdInvalid(request, setNum, tagNum, tempLine);
	}
}

BusResponse::SnoopResult Cache::handleBusRdXInvalid(BusRequest* request, int setNum, int tagNum, CacheLine* tempLine){
	printf("cache number %d does not have set %d and tag %d for address %llx in its cache, ignoring snoop at cycle %llu \n",		processorId, setNum, tagNum, (*request).address, cacheConstants.getCycle());
	return BusResponse::NONE;
}

BusResponse::SnoopResult Cache::handleBusRdXModified(BusRequest* request, int setNum, int tagNum, CacheLine* tempLine){
	//FLUSH LINE TO MEMORY and set the state to invalid
	(*stats).numFlush++;
	printf("cache %d just FLUSH set %d and tag %d \n", processorId, setNum, tagNum);
	(*tempLine).setState(CacheLine::invalid);
	printf("cache number %d just changed set %d and tag %d for address %llx from modified to invalid from a ReadX at cycle %llu \n",		processorId, setNum, tagNum, (*request).address, cacheConstants.getCycle());
	return BusResponse::FLUSH_MODIFIED_TO_INVALID;
}

BusResponse::SnoopResult Cache::handleBusRdXSharedExclusive(BusRequest* request, int setNum, int tagNum, CacheLine* tempLine){
	//invalidate ours
	(*tempLine).setState(CacheLine::invalid);
	printf("cache number %d just changed set %d and tag %d for address %llx from shared (or exclusive) to invalid from a ReadX at cycle %llu \n",		processorId, setNum, tagNum, (*request).address, cacheConstants.getCycle());
	return BusResponse::SHARED;
}

BusResponse::SnoopResult Cache::handleBusRdXMESI(BusRequest* request, int setNum, int tagNum, CacheLine* tempLine){
	if((*tempLine).getState() == CacheLine::shared || (*tempLine).getState() == CacheLine::exclusive){
		return handleBusRdXSharedExclusive(request, setNum, tagNum, tempLine);
	}
	if((*tempLine).getState() == CacheLine::modified){
		return handleBusRdXModified(request, setNum, tagNum, tempLine);
	}
	else{
		return handleBusRdXInvalid(request, setNum, tagNum, tempLine);
	}
}

BusResponse::SnoopResult Cache::handleSnoopMESI(BusRequest* request, int setNum, int tagNum, CacheLine* tempLine){
	BusResponse::SnoopResult result = BusResponse::NONE;
	if((*request).getCommand() == BusRequest::BusRd){
		return handleBusRdMESI(request, setNum, tagNum, tempLine);
	}
	if((*request).getCommand() == BusRequest::BusRdX){
		return handleBusRdXMESI(request, setNum, tagNum, tempLine);
	}
	return result;
}

BusResponse::SnoopResult Cache::handleBusRdXMSI(BusRequest* request, int setNum, int tagNum, CacheLine* tempLine){
	if((*tempLine).getState() == CacheLine::shared){
		return handleBusRdXSharedExclusive(request, setNum, tagNum, tempLine);
	}
	if((*tempLine).getState() == CacheLine::modified){
		return handleBusRdXModified(request, setNum, tagNum, tempLine);
	}
	else{
		return handleBusRdXInvalid(request, setNum, tagNum, tempLine);
	}
}
BusResponse::SnoopResult Cache::handleBusRdMSI(BusRequest* request, int setNum, int tagNum, CacheLine* tempLine){
	if((*tempLine).getState() == CacheLine::shared){
		return handleBusRdShared(request, setNum, tagNum, tempLine);
	}
	if((*tempLine).getState() == CacheLine::modified){
		return handleBusRdModified(request, setNum, tagNum, tempLine);
	}
	else{
		return handleBusRdInvalid(request, setNum, tagNum, tempLine);
	}
}

BusResponse::SnoopResult Cache::handleSnoopMSI(BusRequest* request, int setNum, int tagNum, CacheLine* tempLine){
	BusResponse::SnoopResult result = BusResponse::NONE;
	if((*request).getCommand() == BusRequest::BusRd){
		return handleBusRdMSI(request, setNum, tagNum, tempLine);
	}
	if ((*request).getCommand() == BusRequest::BusRdX)
	{
		return handleBusRdXMSI(request, setNum, tagNum, tempLine);
	}
	return result;
}

BusResponse::SnoopResult Cache::handleSnoopMOESI(BusRequest* request, int setNum, int tagNum, CacheLine* tempLine){
	BusResponse::SnoopResult result = BusResponse::NONE;
	if((*request).getCommand() == BusRequest::BusRd){
		if((*tempLine).getState() == CacheLine::owned){
			//if we have it in owned, we are the one who responds with data
			//no memory use, cacheshare++, miss++
			result = BusResponse::OWNED;
			(*stats).numCacheShare++;
			return result;
		} else if((*tempLine).getState() == CacheLine::modified){
			//if we have it in modified, we are the ones who respond with data, and change line to owned
			//no memory use, cacheshare++, miss++
			(*tempLine).setState(CacheLine::owned);
			(*stats).numCacheShare++;
			result = BusResponse::MODIFIED;
			return result;
		}
		else if ((*tempLine).getState() == CacheLine::shared){
			//we don't give anything to the req cache cause not our job
			//they have to go to main memory
			result = BusResponse::SHARED;
			return result;
		} else if((*tempLine).getState() == CacheLine::exclusive){
			//change to shared, share the data
			result = BusResponse::EXCLUSIVE;
			(*tempLine).setState(CacheLine::shared);
			(*stats).numCacheShare++;
			return result;
		} else{
			//so we're invalid, do nothing
			return result;
		}
	}
	if((*request).getCommand() == BusRequest::BusRdX){
		if((*tempLine).getState() == CacheLine::owned){
			result = BusResponse::OWNED;
			(*stats).numCacheShare++;
			(*tempLine).setState(CacheLine::invalid);
			return result;
		} else if((*tempLine).getState() == CacheLine::modified){
			//we only flush when evicted
			result = BusResponse::MODIFIED; //there is no flush, but can share the data
			(*stats).numCacheShare++;
			(*tempLine).setState(CacheLine::invalid);
			return result;
		}
		else if  ((*tempLine).getState() == CacheLine::exclusive){
			result = BusResponse::EXCLUSIVE;
			(*stats).numCacheShare++;
			(*tempLine).setState(CacheLine::invalid);
			return result;
		}
		else if ((*tempLine).getState() == CacheLine::shared){
			(*tempLine).setState(CacheLine::invalid);
			//don't care about result
		} else{
			//so we're invalid, do nothing
		}
	}
	return result;
}


/*
Read the current BusRequest that another cache issued to the bus
and parse it to see if you need to update our own local cache
*/
BusResponse::SnoopResult Cache::snoopBusRequest(BusRequest* request){

	BusResponse::SnoopResult result = BusResponse::NONE;
	CacheSet* tempSet = localCache[(*request).getSet()];
	int setNum = (*request).getSet();
	int tagNum = (*request).getTag();
	if((*tempSet).hasLine((*request).getTag())){
		//so we do have this line
		CacheLine* tempLine = (*tempSet).getLine((*request).getTag());
		if(cacheConstants.getProtocol() == CacheConstants::MESI){
			BusResponse::SnoopResult result = handleSnoopMESI(request, setNum, tagNum, tempLine);
			busResponse = new BusResponse(result, (*busRequest).getOrderingTime(),(*busRequest).getSenderId());
			responseQueue.push_back(busResponse);
			return result;
		}

		if(cacheConstants.getProtocol() == CacheConstants::MSI){
			BusResponse::SnoopResult result = handleSnoopMSI(request, setNum, tagNum, tempLine);
			busResponse = new BusResponse(result, (*busRequest).getOrderingTime(),(*busRequest).getSenderId());
			responseQueue.push_back(busResponse);
			return result;
		}

		if(cacheConstants.getProtocol() == CacheConstants::MOESI){
			BusResponse::SnoopResult result= handleSnoopMOESI(request, setNum, tagNum, tempLine);
			busResponse = new BusResponse(result, (*busRequest).getOrderingTime(),(*busRequest).getSenderId());
			responseQueue.push_back(busResponse);
			return result;
		}
	}

	busResponse = new BusResponse(result, (*busRequest).getOrderingTime(),(*busRequest).getSenderId());
	responseQueue.push_back(busResponse);
	return result;
}

void Cache::updateEndCycleTime(unsigned long long extraCycleCost){
	jobCycleCost += extraCycleCost;
}

void Cache::newEndCycleTime(unsigned long long decrease){
	jobCycleCost = decrease;
}



/*
Update to store the new line requested
*/
void Cache::busJobDone(bool isShared){
	unsigned long long jobAddr = (*currentJob).getAddress();
	int currJobSet = 0;
	int currJobTag = 0;
	decode_address(jobAddr, &currJobSet, &currJobTag);
	haveBusRequest = false;
	busy = false;
	busRequestBeingServiced = false;
	CacheSet* currSet = localCache[currJobSet];

	//Need to tell if we need to evict a line from the set
	bool needToEvict = (*currSet).isFull() && !((*currSet).hasLine(currJobTag));
	if (needToEvict){
		(*currSet).evictLRULine();
		(*stats).numEvict++;
		printf("just evicted a line \n");
	}

	if (!(*currSet).hasLine(currJobTag)){
		CacheLine* newLine = new CacheLine(jobAddr, currJobSet, currJobTag);
		(*currSet).addLine(newLine);
	}

	CacheLine* currLine = (*currSet).getLine(currJobTag); 
	(*currLine).lastUsedCycle = cacheConstants.getCycle();
	if((*currentJob).isWrite()){
		if(isShared && cacheConstants.getProtocol() == CacheConstants::MOESI){
			//we already calculated if it was a share or not
			(*currLine).setState(CacheLine::modified);
			//share in this case means that we got the data from a modified or an owned cache or an exclusive
		}
		else if (cacheConstants.getProtocol() == CacheConstants::MOESI && (*currLine).getState() != CacheLine::owned){
			//so it wasn't shared
			printf("~~~ write was from mem \n");
			(*stats).numMainMemoryUses++;
			(*currLine).setState(CacheLine::modified);
		}
		else if(isShared && cacheConstants.getProtocol() == CacheConstants::MESI){
			(*stats).numCacheShare++;
			printf("~~~~~~~ share++ \n");
		}
		else if( cacheConstants.getProtocol() == CacheConstants::MSI){
			//msi protocol, we had to read from memory
			(*stats).numMainMemoryUses++;
			printf("~~~~~~~~~~~~ mem use ++ \n");
		}
		else{
		}
		(*currLine).setState(CacheLine::modified);
		printf("cache %d has just been told it has finished a job for address %llx and stored in modified state at cycle %llu \n",
			processorId, (*currentJob).getAddress(), cacheConstants.getCycle());
	}
	if((*currentJob).isRead()){
		if(cacheConstants.getProtocol() == CacheConstants::MOESI){
			if(isShared){
				//so someone was modified or owned or exclusive
				printf("just got my read request as a share \n");
				(*currLine).setState(CacheLine::shared);
			}
			else{
				//so we exclusive, and had to get from main memory
				printf("my read req was exclusive ~~~~~~~~~~ \n");
				(*currLine).setState(CacheLine::exclusive);
				(*stats).numMainMemoryUses++;
			}
		}
		if((cacheConstants).getProtocol() == CacheConstants::MESI){
			if(!isShared){
				(*currLine).setState(CacheLine::exclusive);
				(*stats).numMainMemoryUses++;
				printf("main mem ++");
				printf("cache %d has just been told it has finished a job for address %llx and stored in exclusive state at cycle %llu \n", 
					processorId, (*currentJob).getAddress(), cacheConstants.getCycle());
			}
			else{
				(*currLine).setState(CacheLine::shared);
				(*stats).numCacheShare++;
				printf("share ++ \n");
				printf("cache %d has just been told it has finished a job for address %llx and stored in shared state at cycle %llu \n", 
					processorId, (*currentJob).getAddress(), cacheConstants.getCycle());
			}
		}
		if(cacheConstants.getProtocol() == CacheConstants::MSI){
			(*stats).numMainMemoryUses++;
			printf("mem++ ~~~~~~~~~~~~~~~ \n");
			(*currLine).setState(CacheLine::shared);
			printf("cache %d has just been told it has finished a job for address %llx and stored in shared state at cycle %llu \n", 
				processorId, (*currentJob).getAddress(), cacheConstants.getCycle());
		}
	}
}



int Cache::getProcessorId(){
	return processorId;
}

void Cache::updateCurrentJobLineCycle(){
	int set = 0;
	int tag = 0;
	if(currentJob == NULL){
		return;
	}
	decode_address((*currentJob).getAddress(), &set, &tag);
	for(unsigned int i = 0; i < localCache.size(); i++){
		if((localCache[i] != NULL) && (*localCache[i]).hasLine(tag)){
			CacheLine* theLine = (*localCache[i]).getLine(tag);
			(*theLine).lastUsedCycle = cacheConstants.getCycle();
		}
	}

}
std::vector<BusRequest*> Cache::getBusRequestQueue(){
	return requestQueue;
}
std::vector<BusResponse*> Cache::getBusResponseQueue(){
	return responseQueue;
}

void Cache::tick(){

	if(busRequestBeingServiced){
		return; 
		//we have to wait for the bus to tell us we're done, not our own selves saying it
		//since cache time incremented before bus time
	}

	if(startServiceCycle + jobCycleCost <= cacheConstants.getCycle()){
		//finished a job
		updateCurrentJobLineCycle();
		busy = false;
	}

	if(!busy && pendingJobs.size() != 0){
		//so we're free to do a new request
		handleRequest();
	}
}

Cache::~Cache(void){

}
