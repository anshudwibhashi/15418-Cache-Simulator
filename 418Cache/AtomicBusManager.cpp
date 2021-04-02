#include "AtomicBusManager.h"
#include "CacheConstants.h"
#include "Cache.h"
#include "vector"
#include "CacheJob.h"
#include "BusRequest.h"
#include "BusResponse.h"
#include <algorithm>

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

void AtomicBusManager::handleBusResponse(BusResponse* response){
	BusResponse::SnoopResult result = response->getResult();
	if(constants.getProtocol() == CacheConstants::MSI){
		if (result == BusResponse::FLUSH_MODIFIED_TO_SHARED || result == BusResponse::FLUSH_MODIFIED_TO_INVALID)
		{
			isShared = true;
		}
	}
	if(constants.getProtocol() == CacheConstants::MOESI){
		if(result == BusResponse::MODIFIED || result == BusResponse::EXCLUSIVE || result == BusResponse::OWNED){
			isShared = true;
		}
	}

	if(constants.getProtocol() == CacheConstants::MESI){
		if(result == BusResponse::FLUSH_MODIFIED_TO_INVALID){
			isShared = true;
		}
		if(result == BusResponse::FLUSH_MODIFIED_TO_SHARED){
			isShared = true;
		}
		if(result == BusResponse::SHARED){
			isShared = true;
		}
	}
}

//Handle checking if the current BusRequest is completed
//And if so, getting a new one and broadcasting it to all other caches
void AtomicBusManager::tick(){

	if(inUse){
		//so the current job being executed is completed this cycle 
		if(endCycle <= constants.getCycle()){
			if (currentResponse != NULL) {
				handleBusResponse(currentResponse);

				if (isShared) { (*caches.at(currentCache)).busJobDone(isShared); }
				currentResponse = NULL;
			}
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
	for(int i = 0; i < 2; i++){	
		if((caches.at(i)) != NULL){
			int otherCacheId = caches.at(i == 0 ? 1 : 0)->getProcessorId();
			BusResponse* response = caches.at(i)->getResponseForSender(otherCacheId);
			if (response == NULL || response->getResult() == BusResponse::NONE) continue;

			currentResponse = response;

			startCycle = constants.getCycle();
			endCycle = startCycle + propagationDelay; 
			inUse = true;
			return;
		}
	}
	
	for(int i = 0; i < 2; i++){	
		if(((caches.at(i)) != NULL) && (*caches.at(i)).hasBusRequest()){
			//so we will now service this cache
			bool haveRequestAlready = false;
			if (i == 0 && std::find(outstandingRequestsFromA.begin(), outstandingRequestsFromA.end(), (*caches.at(i)).getBusRequest()->getOrderingTime()) != outstandingRequestsFromA.end()) { haveRequestAlready = true; }
			if (i == 1 && std::find(outstandingRequestsFromB.begin(), outstandingRequestsFromB.end(), (*caches.at(i)).getBusRequest()->getOrderingTime()) != outstandingRequestsFromB.end()) { haveRequestAlready = true; }
			if (haveRequestAlready) { continue; }

			currentRequest = (*caches.at(i)).getBusRequest();
			if (i == 0) { outstandingRequestsFromA.push_back(currentRequest->getOrderingTime()); }
			else { outstandingRequestsFromB.push_back(currentRequest->getOrderingTime()); }
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
	printf("now servicing cache %d on the bus at cycle %llu \n", (*caches.at(currentCache)).getProcessorId(), constants.getCycle());
	(*stats).numBusRequests++;
	//since only get here if we got a new job
	//update the startCycle for when we just changed jobs
	startCycle = constants.getCycle();
	endCycle = startCycle + propagationDelay; 
	inUse = true;

	//so now we have the new currentRequest and currentCache is the cache that asked for that request
	//so now we broadcast this currentRequest to all the caches other than the one who sent it
	for(int i = 0; i < 2; i++){
		if(i != currentCache){
			(*caches.at(i)).snoopBusRequest(currentRequest);
		}
	}
}

AtomicBusManager::~AtomicBusManager(void){
}
