#pragma once
#include "CacheConstants.h"
#include "CacheJob.h"
#include "queue"
#include "BusRequest.h"
#include "CacheConstants.h"
#include "CacheController.h"	
#include "CacheSet.h"
#include "vector"
#include "CacheJob.h"
#include "queue"
#include "BusRequest.h"
#include "CacheLine.h"
#include "CacheStats.h"
#include "BusResponse.h"

class Cache
{
public:

	CacheConstants cacheConstants;
	std::vector<CacheSet*> localCache;
	std::queue<CacheJob*> pendingJobs;
	CacheJob* currentJob;
	BusRequest* busRequest;
	// add BusResponse and busResponse queue; 
	BusResponse* busResponse;
	std::vector<BusResponse*> responseQueue;

	CacheStats* stats;
	int processorId;
	bool haveBusRequest;
	bool busy;
	bool busRequestBeingServiced;
	unsigned long long startServiceCycle;
	unsigned long long jobCycleCost;
	CacheLine::State currentBusJobResultState;

	void updateEndCycleTime(unsigned long long);
	typedef enum {SHARED, FLUSH_MODIFIED_TO_SHARED,FLUSH_MODIFIED_TO_INVALID, EXCLUSIVE, OWNED, MODIFIED, NONE} SnoopResult;	 
	Cache(int, CacheConstants,std::queue<CacheJob*>*, CacheStats*);
	int getProcessorId();
	void setPId(int);
	void handleRequest();
	void tick();
	void busJobDone(bool);
	bool hasBusRequest();
	void decode_address(unsigned long long address, int* whichSet, int* tag);
	unsigned long long getTotalMemoryCost(int set, int tag);
	unsigned long long getOrderingTime();
	bool lineInState(CacheLine::State state);
	void setLineState(CacheLine::State state);
	void updateCurrentJobLineCycle();
	BusRequest* getBusRequest();
	vector<BusResponse*> getBusResponseQueue();
	Cache::SnoopResult snoopBusRequest(BusRequest*);
	void newEndCycleTime(unsigned long long);
	~Cache(void);
private:

	
	void handleWriteRequestMESI();
	void handleReadRequestMESI();
	void handleWriteRequestMSI();
	void handleReadRequestMSI();
	bool handleWriteModified();
	bool handleWriteExclusive();
	void handleWriteSharedInvalid();
	bool handleReadHit();
	void handleReadMiss();
	Cache::SnoopResult handleSnoopMESI(BusRequest*, int, int, CacheLine*);
	Cache::SnoopResult handleSnoopMSI(BusRequest*, int, int, CacheLine*);
	Cache::SnoopResult handleSnoopMOESI(BusRequest*, int, int, CacheLine*);
	Cache::SnoopResult handleBusRdShared(BusRequest*, int, int, CacheLine*);
	Cache::SnoopResult handleBusRdModified(BusRequest*, int, int, CacheLine*);
	Cache::SnoopResult handleBusRdInvalid(BusRequest*, int, int, CacheLine*);
	Cache::SnoopResult handleBusRdXInvalid(BusRequest*, int, int, CacheLine*);
	Cache::SnoopResult handleBusRdXModified(BusRequest*, int, int, CacheLine*);
	Cache::SnoopResult handleBusRdXSharedExclusive(BusRequest*, int, int, CacheLine*);
	Cache::SnoopResult handleBusRdXMESI(BusRequest*, int, int, CacheLine*);
	Cache::SnoopResult handleBusRdMESI(BusRequest*, int, int, CacheLine*);
	Cache::SnoopResult handleBusRdXMSI(BusRequest*, int, int, CacheLine*);
	Cache::SnoopResult handleBusRdMSI(BusRequest*, int, int, CacheLine*);

};

