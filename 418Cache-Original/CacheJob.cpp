#include "CacheJob.h"

CacheJob::CacheJob(){
}

CacheJob::CacheJob(char readWrite, unsigned long long addr, int tId)
{
	rW = readWrite;
	address = addr;
	threadId = tId;
}

bool CacheJob::isRead(){
	return (rW == 'R');
}

bool CacheJob::isWrite(){
	return (rW == 'W');
}

unsigned long long CacheJob::getAddress(){
	return address;
}

int CacheJob::getThreadId(){
	return threadId;
}

CacheJob::~CacheJob(void)
{
}
