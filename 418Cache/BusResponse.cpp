#include "BusResponse.h"

BusResponse::BusResponse(SnoopResult result, unsigned long long ordtime, int senderid){
    result = result;
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