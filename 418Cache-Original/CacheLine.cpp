#include "CacheLine.h"
#include "CacheConstants.h"

/*
Represents a single line in the cache.
*/
CacheLine::CacheLine(unsigned long long address, int set, int tag)
{
	rawAddress = address;
	setIndex = set;
	myTag = tag;
	myState = invalid;
	lastUsedCycle = 0;
}

CacheLine::State CacheLine::getState(){
	return myState;
}

void CacheLine::setState(State state)
{
	myState = state;
}

unsigned long long CacheLine::getAddress(){
	return rawAddress;
}

int CacheLine::getSetIndex(){
	return setIndex;
}

int CacheLine::getTag(){
	return myTag;
}

CacheLine::~CacheLine(void)
{
}
