#pragma once
#include <cstdint>
#include <ostream>



#pragma pack(push, 1)
struct RawUpdate {
    uint64_t timestamp; 
    char type;          
    char side;          
    double price;       
    double quantity;    
};
#pragma pack(pop)



struct alignas(64) MarketUpdate {
    uint64_t timestamp; 
    double price;
    double quantity;
    int32_t symbol_id; 
    char type;         
    char side;
    
    
    static MarketUpdate from_raw(const RawUpdate& raw) {
        MarketUpdate u;
        u.timestamp = raw.timestamp;
        u.type = raw.type;
        u.side = raw.side;
        u.price = raw.price;
        u.quantity = raw.quantity;
        u.symbol_id = 0; 
        return u;
    }
};