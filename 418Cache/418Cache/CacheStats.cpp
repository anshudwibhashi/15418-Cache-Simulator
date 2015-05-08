#include "CacheStats.h"

/*
Records information such as:
number of hits
number of misses
number of flushes
false sharing somehow?
*/
CacheStats::CacheStats(void)
{
	numFlush = 0;
	numHit	= 0;
	numMiss = 0;
	numEvict = 0;
}


CacheStats::~CacheStats(void)
{
}
