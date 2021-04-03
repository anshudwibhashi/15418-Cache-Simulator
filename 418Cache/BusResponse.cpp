#include "BusResponse.h"

BusResponse::BusResponse(SnoopResult res, unsigned long long ordtime, int senderid){
    result = res;
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
    return result;
}