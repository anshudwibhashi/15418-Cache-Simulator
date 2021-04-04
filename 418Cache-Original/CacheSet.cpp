#include "CacheSet.h"
#include "vector"
#include "CacheLine.h"
#include "CacheConstants.h"
#include "limits.h"


/*
Container of the lines for an individual set.
Manages requests for lines and LRU eviction
*/
CacheSet::CacheSet(CacheConstants* constants)
{
	consts = constants;
}

bool CacheSet::isFull()
{
	if(allLines.size() < (*consts).getNumLinesInSet()){
		return false;
	}
	for (int i = 0; i < allLines.size(); ++i)
	{
		if (allLines[i] == NULL || ((*allLines[i]).getState() != CacheLine::modified))
		{
			return false;
		}
	}
	return true;
}

void CacheSet::addLine(CacheLine* line){
	if(allLines.size() == (*consts).getNumLinesInSet()){
		//so have to evict before adding this line
		evictLRULine();
	}
	allLines.push_back(line);
}

bool CacheSet::hasLine(int tag){
	for(int i = 0; i < allLines.size(); i++){
		if((allLines[i] != NULL) && (*allLines[i]).getTag() == tag){
			return true;
		}
	}
	return false;
}

//return the line with this tag
//assumes line with that tag is in the set
CacheLine* CacheSet::getLine(int tag){
	for(int i = 0; i < allLines.size(); i++){
		if((allLines[i] != NULL) && (*allLines[i]).getTag() == tag){
			//update when used
			(*allLines[i]).lastUsedCycle = (*consts).getCycle();
			return allLines[i];
		}
	}
	return NULL;
}

//true if the line we're evicting is modified, false otherwise
bool CacheSet::evictLineModified(){
	int lineToEvict = 0;
	unsigned long long leastRecentCycle = ULLONG_MAX;

	if (allLines.size() != (*consts).getNumLinesInSet())
		return false;
	printf("handleWriteSharedInvalid\n");

	for (int i = 0; i < allLines.size(); ++i)
	{
		if ((allLines[i] != NULL) && (*allLines[i]).lastUsedCycle < leastRecentCycle)
		{
			leastRecentCycle = (*allLines[i]).lastUsedCycle;
			lineToEvict = i;
		}
	}
	if((*allLines[lineToEvict]).getState() == CacheLine::modified){
		return true;
	}
	else{
		return false;
	}
}

/*
Remove the oldest line in the set
*/
void CacheSet::evictLRULine()
{
	unsigned long long leastRecentCycle = ULLONG_MAX;
	int lineToEvict;
	for (int i = 0; i < allLines.size(); ++i)
	{
		if ((allLines[i] != NULL) && (*allLines[i]).lastUsedCycle < leastRecentCycle)
		{
			leastRecentCycle = (*allLines[i]).lastUsedCycle;
			lineToEvict = i;
		}
	}
	printf("rip line %llx \n", (*allLines[lineToEvict]).getAddress());
	allLines.erase(allLines.begin() + lineToEvict);
}

CacheSet::~CacheSet(void)
{
}
