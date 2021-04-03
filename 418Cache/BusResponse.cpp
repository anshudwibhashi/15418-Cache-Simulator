#include "BusResponse.h"

BusResponse::BusResponse(BusResponse::SnoopResult result, unsigned long long ordtime, int senderid){
    res = result;
    ordTime = ordtime;
    senderId = senderid;
}

unsigned long long BusResponse::getOrdTime(){
    return ordTime;
}

int BusResponse::getSenderId(){
    return senderId;
}

BusResponse::SnoopResult BusResponse::getResult(){
    return res;
}
BusResponse::~BusResponse(void)
{
}