#pragma once

class BusResponse{
    public:
        typedef enum {SHARED, FLUSH_MODIFIED_TO_SHARED,FLUSH_MODIFIED_TO_INVALID, EXCLUSIVE, OWNED, MODIFIED, NONE} SnoopResult;	       
        SnoopResult res;
        unsigned long long ordTime;
        int senderId;
        BusResponse(SnoopResult, unsigned long long, int);
        unsigned long long getOrdTime();
        int getSenderId();
        SnoopResult getResult();

        ~BusResponse(void);
};