#pragma once
#include "CacheConstants.h"
#include "Cache.h"
#include "vector"
#include "CacheStats.h"
#include "BusResponse.h"

class AtomicBusManager
{
public:
	AtomicBusManager(CacheConstants, std::vector<Cache*>* , CacheStats* stats, int propagationDelay);
	//current cache that had/has bus access
	int currentCache;
	//current bus request being served
	BusRequest* currentRequest;
	BusResponse* currentResponse;
	bool inUse;
	//which cycle we started a job on
	unsigned long long startCycle; 
	//what cycle we will end a job on
	unsigned long long endCycle;
	CacheConstants constants;
	//current job being worked on that has the bus access
	CacheJob currentJob;
	//list of all the caches in the system
	std::vector<Cache*> caches;
	//result of if a line for a busrequest is shared or not
	bool isShared;
	CacheStats* stats;
	int propagationDelay;
	void tick(void);
	void handleBusResponse(BusResponse*);
	~AtomicBusManager(void);
	std::vector<unsigned long long> outstandingRequestsFromA;
	std::vector<unsigned long long> outstandingRequestsFromB;
};

