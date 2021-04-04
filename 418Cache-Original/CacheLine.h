#pragma once
#include "CacheConstants.h"
class CacheLine
{
public:
	typedef enum {invalid, shared, modified, exclusive, owned} State;
	unsigned long long rawAddress; //raw address that maps to me (ignoring block offset)
	int setIndex; //what set i'm in
	int myTag; //unique tag identifier
	CacheLine::State myState;
	unsigned long long lastUsedCycle;
	CacheLine(unsigned long long, int, int);
	unsigned long long getAddress();
	int getTag();
	int getSetIndex();
	State getState();
	void setState(State state);
	~CacheLine(void);
};

