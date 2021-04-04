#include "CacheConstants.h"
#include "string"

int cacheHitCycleCost;
int memoryResponseCycleCost;
int propagationDelayRectLength; // Formerly: int cacheResponseCycleCost;
int propagationDelayRectWidth; 
int propagationDelayRectDiagonal; 
int numProcessors;

int numSets;
int numSetBits;
int numLinesInSet;
int numBytesInLine;
int numBytesBits;
int numAddressBits; //how many bits are the addresses

int numCacheSize; //= 2^sets * 2^lines * bytesPerLine

unsigned long long cycles; //total count of all cycles elapsed in simulation

CacheConstants::Protocol protocol; //string representing what the protocol is 

//i don't care what the blockoffset actually is, because i don't care what the data actually is

//so an address is: sets+ (log base 2 of bytesPerLine)

//Intel i7 has 32k, 8 way associative, 64 byte line 
//size = # sets * #lines per set (8 in this case) * 64
//S = 2^s sets
//E = 2^e lines per set
//B = 2^b bytes per block


CacheConstants::CacheConstants(void)
{
	cacheHitCycleCost = 4;
	memoryResponseCycleCost = 100;
	propagationDelayRectLength = 20; 
	propagationDelayRectWidth = 21;
	propagationDelayRectDiagonal = 29;
	numProcessors = 4;
	numSets = 64;  //totalCacheSize / (numLinesInSet * (numBytesInLine));
	numSetBits = 6; //2^ 6 = 64
	numLinesInSet = 8; //8 way associative
	numBytesInLine = 64; //64 bytes per line
	numBytesBits = 6; // 2^6 == 64
	numCacheSize = (numSets * numLinesInSet * numBytesInLine); //~32k
	numAddressBits = 48;
	cycles = 0;
	protocol = CacheConstants::MSI;
}

int CacheConstants::getPropagationDelayRectLength(){
	return propagationDelayRectLength;
}

int CacheConstants::getPropagationDelayRectWidth(){
	return propagationDelayRectWidth;
}

int CacheConstants::getPropagationDelayRectDiagonal(){
	return propagationDelayRectDiagonal;
}

int CacheConstants::getNumAddressBits(){
	return numAddressBits;
}


CacheConstants::Protocol CacheConstants::getProtocol(){
	return protocol;
}

int CacheConstants::getNumSetBits(){
	return numSetBits;
}

int CacheConstants::getNumBytesBits(){
	return numBytesBits;
}

int CacheConstants::getCacheHitCycleCost(){
	return cacheHitCycleCost;
}
int CacheConstants::getMemoryResponseCycleCost(){
	return memoryResponseCycleCost;
}
int CacheConstants::getNumProcessors(){
	return numProcessors;
}
int CacheConstants::getNumSets(){
	return numSets;
}
int CacheConstants::getNumLinesInSet(){
	return numLinesInSet;
}
int CacheConstants::getNumBytesInLine(){
	return numBytesInLine;
}
int CacheConstants::getNumCacheSize(){
	return numCacheSize;
}

unsigned long long CacheConstants::getCycle(){
	return cycles;
}

void CacheConstants::tick(){
	cycles++;
}




CacheConstants::~CacheConstants(void)
{
}
