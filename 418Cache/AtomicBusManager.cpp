#include "AtomicBusManager.h"
#include "CacheConstants.h"
#include "Cache.h"
#include "vector"
#include "CacheJob.h"
#include "BusRequest.h"
#include "BusResponse.h"

AtomicBusManager::AtomicBusManager(CacheConstants consts, std::vector<Cache*>* allCaches, CacheStats* statTracker, int propagationDelay)
{
	constants = consts;
	caches = *allCaches;
	currentCache = 0;
	startCycle = 0;
	endCycle = 0;
	inUse = false;
	isShared = false;
	stats = statTracker;
	this->propagationDelay = propagationDelay;
}

//Handle checking if the current BusRequest is completed
//And if so, getting a new one and broadcasting it to all other caches
void AtomicBusManager::tick(){

	if(inUse){
		//so the current job being executed is completed this cycle 
		if(endCycle <= constants.getCycle()){
			//tell the cache that its job is done
			(*caches.at(currentCache)).busJobDone(isShared);
			currentCache = -1;
			inUse = false;
			isShared = false;
		}
		else{
			return;
		}
	}

	int tempNextCache = -1;
	//so either not in use, or we just finished a job
	// < 2 instead of < constants.getNumProcessors() because each link connects 2 procs now
	for(int i = 0; i < 2; i++){	
		if(((caches.at(i)) != NULL) && (*caches.at(i)).hasBusRequest()){
			//so we will now service this cache
			currentRequest = (*caches.at(i)).getBusRequest();
			tempNextCache = i;
			break;
		}
	}

	if(tempNextCache == -1){
		//so there are no more pending requests in the system
		//printf("no one to service, leaving \n");
		inUse = false;
		isShared = false;
		return;
	}
	currentCache = tempNextCache;
	printf("now servicing cache %d on the bus at cycle %llu \n", currentCache, constants.getCycle());
	(*stats).numBusRequests++;
	//since only get here if we got a new job
	//update the startCycle for when we just changed jobs
	startCycle = constants.getCycle();
	endCycle = startCycle + (*currentRequest).getCycleCost(); 
	inUse = true;

	bool foundShared = false;
	//so now we have the new currentRequest and currentCache is the cache that asked for that request
	//so now we broadcast this currentRequest to all the caches other than the one who sent it
	for(int i = 0; i < 2; i++){
		if(i != currentCache){
			// Add the busResponse 
			BusResponse::SnoopResult result = (*caches.at(i)).snoopBusRequest(currentRequest);
			if(constants.getProtocol() == CacheConstants::MSI){
				if (result == BusResponse::FLUSH_MODIFIED_TO_SHARED || result == BusResponse::FLUSH_MODIFIED_TO_INVALID)
				{
					//flush to memory, then load from memory
					endCycle += constants.getMemoryResponseCycleCost();
					(*caches[currentCache]).updateEndCycleTime(constants.getMemoryResponseCycleCost());
					//make sure the cache itself knows that it isn't finished until the proper time
					(*stats).numMainMemoryUses++;
					printf("num mem use ++ \n");
					continue;
				}
				if (result == BusResponse::SHARED){
					//do nothing, no sharing in MSI
					continue;
				}
				if (result == BusResponse::NONE){
					//Do nothing
					continue;
				}
			}
			if(constants.getProtocol() == CacheConstants::MOESI){
				if(result == BusResponse::MODIFIED || result == BusResponse::EXCLUSIVE || result == BusResponse::OWNED){
					//so adjust the cycle cost to a share
					//but don't adjust cost if it was an upgrade from owned to modified, cause that's bad
					if((*currentRequest).getCycleCost() == constants.getCacheHitCycleCost()){
						//so was a bus upgrade essentially
						isShared = true;
						continue;
					}
					else{
						endCycle -= (constants.getMemoryResponseCycleCost() - propagationDelay);
						(*caches[currentCache]).newEndCycleTime(propagationDelay);
						isShared = true;
					}
				}
			}

			if(constants.getProtocol() == CacheConstants::MESI){
				if(result == BusResponse::FLUSH_MODIFIED_TO_INVALID){
					endCycle += propagationDelay;
					(*caches[currentCache]).updateEndCycleTime(propagationDelay);
					isShared = true;
					//so requesting cache should set the line to modified
					//this happens from a busrdx command
					(*stats).numMainMemoryUses++;
					printf("num main mem use ++ \n");
					continue;
				}
				if(result == BusResponse::FLUSH_MODIFIED_TO_SHARED){
					//so this happens when there is a busrd req
					endCycle += propagationDelay;
					(*caches[currentCache]).updateEndCycleTime(propagationDelay);
					//so tell the cache that it shoudl set the line to shared
					isShared = true;
					(*stats).numMainMemoryUses++;
					printf("num main mem use ++ \n");
					continue;
				}
				if(result == BusResponse::SHARED){
					if(!foundShared){
						endCycle -= (constants.getMemoryResponseCycleCost() - propagationDelay);
						(*caches[currentCache]).newEndCycleTime(propagationDelay);
						foundShared = true;
					}
					isShared = true;
					continue;
				}
				if(result == BusResponse::NONE){
					//nothing
					continue;
				}
			}
		}
		//so now all caches have acknowledged the new BusRequest that was issued
	}
	if(!foundShared && (constants.getProtocol() == CacheConstants::MESI)){
		//so if we never did get a shared, had to get from main memoryu
		(*stats).numMainMemoryUses++;
	}
}

AtomicBusManager::~AtomicBusManager(void){
}
