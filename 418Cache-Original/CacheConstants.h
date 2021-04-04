#pragma once
#include "string"
class CacheConstants
{
public:
	CacheConstants(void);
	typedef enum {MSI, MESI, MOESI} Protocol;
	int getCacheHitCycleCost();
	int getMemoryResponseCycleCost();
	int getNumProcessors();
	int getNumSets();
	int getNumLinesInSet();
	int getNumBytesInLine();
	int getNumCacheSize();
	unsigned long long getCycle();
	void tick();
	Protocol getProtocol();
	int getNumBytesBits();
	int getNumSetBits();
	int getNumAddressBits();
	int getCacheResponseCycleCost();


	~CacheConstants(void);
};

