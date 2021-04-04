#pragma once
#include "CacheConstants.h"
#include "Cache.h"
#include "vector"
#include "CacheStats.h"
class AtomicBusManager
{
public:
	AtomicBusManager(CacheConstants, std::vector<Cache*>* , CacheStats* stats);
	//current cache that had/has bus access
	int currentCache;
	//current bus request being served
	BusRequest* currentRequest;
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
	void tick(void);
	~AtomicBusManager(void);
};

