#include "CacheController.h"
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include "CacheConstants.h"
#include "Cache.h"
#include <queue>
#include "AtomicBusManager.h"
#include "CacheStats.h"


CacheController::CacheController(void)
{
}


CacheController::~CacheController(void)
{
}

bool queuesEmpty(std::vector<Cache*> caches){
	bool allEmpty = true;
	for(int i = 0; i < caches.size(); i++){
		if((*caches[i]).pendingJobs.size() != 0){
			allEmpty = false;
		}
	}
	return allEmpty;
}

bool noJobs(std::vector<Cache*> caches){
	bool allEmpty = true;
	for(int i = 0; i < caches.size(); i++){
		if((*caches[i]).busy){
			allEmpty = false;
		}
	}
	return allEmpty;

}

int main(int argc, char* argv[]){
	CacheConstants constants;
	//local var so don't have to do an object reference each time
	int numProcessors = constants.getNumProcessors();
	int accessProcessorId = 0;
	char readWrite = ' ';
	unsigned long long address = 0;
	unsigned int threadId = 0;
	std::string line;
	AtomicBusManager* bus;
	std::vector<Cache*> caches;
	CacheStats* stats = new CacheStats();

	//keep track of all jobs that the processors have to do
	std::queue<CacheJob*> outstandingRequests; 


	//std::vector<std::queue<CacheJob*>> outstandingRequests (numProcessors);

	char* filename = argv[1];
	if(filename == NULL){
		printf("Error, no filename given");
		exit(0);
	}
	std::ifstream tracefile(filename);
	if(!tracefile){
		printf("Error opening the tracefile, try again");
		exit(0);
	}

	while(getline(tracefile, line)){
		//so while there are lines to read from the trace
		sscanf(line.c_str(), "%c %llx %u", &readWrite, &address, &threadId);
		accessProcessorId = (threadId % numProcessors);
		//so accessProcessorId is now the # of the cache that is responsible for the thread

		outstandingRequests.push(new CacheJob(readWrite, address, accessProcessorId));

		//outstandingRequests.at(accessProcessorId).push(new CacheJob(readWrite, address, threadId));

		printf("rw:%c addr:%llX threadId:%d \n", readWrite, address, accessProcessorId);
	}

	/*
	//incorrect scheduler for now...

	for(int i = 0; i < constants.getNumProcessors(); i++){
	caches.push_back(new Cache(i, constants, &outstandingRequests.at(i), stats));
	}

	//so now all queues are full with the jobs they need to run
	bus = new AtomicBusManager(constants, &caches);

	while(!queuesEmpty(caches)){
	//time must first increment for the constants
	constants.tick();
	//then call for all the caches
	for(int j = 0; j < numProcessors; j++){
	//printf("trying to tick for cache %d, actually ticking cache %d \n", j, (*caches.at(j)).getProcessorId());
	(*caches.at(j)).tick();
	}
	//then call the bus manager
	(*bus).tick();

	}
	*/




	//Creating all of the caches and putting them into the caches vector
	for(int i = 0; i < constants.getNumProcessors(); i++){
		std::queue<CacheJob*> tempQueue;
		caches.push_back(new Cache(i, constants, &tempQueue, stats));
	}

	//so now all queues are full with the jobs they need to run
	bus = new AtomicBusManager(constants, &caches, stats);

	while(!noJobs(caches) || !outstandingRequests.empty()){
		//time must first increment for the constants
		constants.tick();
		//then call for all the caches
		if(noJobs(caches))
		{
			printf("at cycle %llu we process a new job \n", constants.getCycle());
			CacheJob* currJob = outstandingRequests.front();
			outstandingRequests.pop();
			int currThread = (*currJob).getThreadId();
			((*(caches[currThread])).pendingJobs).push(currJob);
		}
		for(int j = 0; j < numProcessors; j++){
			(*caches.at(j)).tick();
		}
		//then call the bus manager
		(*bus).tick();

	}

	fprintf(stderr, "finished at cycle %llu \n", constants.getCycle());
	fprintf(stderr, "num hits: %llu num miss: %llu num flush: %llu num evicts: %llu num bus request: %llu num shares: %llu num Ex2Mod: %llu, num main memory use: %llu \n", 
		(*stats).numHit, (*stats).numMiss, (*stats).numFlush, (*stats).numEvict, (*stats).numBusRequests, (*stats).numCacheShare, (*stats).numExclusiveToModifiedTransitions, (*stats).numMainMemoryUses);

	for(int i = 0; i < numProcessors; i++){
		delete caches[i];
	}
	tracefile.close();

}