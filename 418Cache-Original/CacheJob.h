#pragma once
class CacheJob
{
public:
	CacheJob(char, unsigned long long, int);
	char rW;
	unsigned long long address;
	int threadId;
	CacheJob();
	~CacheJob(void);
	bool isRead();
	bool isWrite();
	int getThreadId();
	int getCycleCost(); //return the number of cycles needed for this request to be serviced
	unsigned long long getAddress();
};

