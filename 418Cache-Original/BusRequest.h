#pragma once
class BusRequest
{
public:
	typedef enum {BusRd, BusRdX} BusMessage; 
	BusRequest::BusMessage command;
	int lineSet;
	int lineTag;
	int cycleCost;
	unsigned long long address;
	BusRequest(BusMessage, int, int, int, unsigned long long);
	BusMessage getCommand();
	int getSet();
	int getTag();
	int getCycleCost();
	~BusRequest(void);
};

