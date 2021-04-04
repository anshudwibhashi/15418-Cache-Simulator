#pragma once
#include "CacheConstants.h"
#include "CacheLine.h"
#include "vector"

class CacheSet
{
public:
	std::vector<CacheLine*> allLines;
	CacheConstants* consts;
	CacheSet(CacheConstants* );
	bool hasLine(int);
	CacheLine* getLine(int);
	~CacheSet(void);
	bool isFull();
	void evictLRULine();
	bool evictLineModified();
	void addLine(CacheLine*);
};

