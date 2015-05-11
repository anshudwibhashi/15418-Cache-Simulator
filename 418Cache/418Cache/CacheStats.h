#pragma once
class CacheStats
{
public:
	unsigned long long numHit;
	unsigned long long numMiss;
	unsigned long long numFlush;
	unsigned long long numEvict;
	unsigned long long numCacheShare;
	unsigned long long numMainMemoryUses;
	unsigned long long numExclusiveToModifiedTransitions;
	unsigned long long numBusRequests;
	unsigned long long numWrites;
	unsigned long long numReads;
	CacheStats(void);
	~CacheStats(void);
};

