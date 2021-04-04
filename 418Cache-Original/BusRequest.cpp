#include "BusRequest.h"


BusRequest::BusRequest(BusRequest::BusMessage busCommand, int set, int tag, int jobCycleCost, unsigned long long adr){
	command = busCommand;
	lineSet = set;
	lineTag = tag;
	cycleCost = jobCycleCost;
	address = adr;
}

BusRequest::BusMessage BusRequest::getCommand(){
	return command;
}

int BusRequest::getSet(){
	return lineSet;
}

int BusRequest::getTag(){
	return lineTag;
}

int BusRequest::getCycleCost(){
	return cycleCost;
}


BusRequest::~BusRequest(void)
{
}
