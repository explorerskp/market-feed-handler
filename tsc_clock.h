#pragma once
#include <cstdint>
#include <thread>
#include <chrono>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif


class TSCClock {
public:
    double ns_per_tick;

    TSCClock() {
        calibrate();
    }

    
    inline static uint64_t now() {
        
        
        _mm_lfence(); 
        return __rdtsc();
    }

    
    inline long to_ns(uint64_t cycles) const {
        return (long)(cycles * ns_per_tick);
    }

private:
    void calibrate() {
        auto t1 = std::chrono::high_resolution_clock::now();
        uint64_t c1 = __rdtsc();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        uint64_t c2 = __rdtsc();
        auto t2 = std::chrono::high_resolution_clock::now();

        double ns_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
        double cycles_elapsed = c2 - c1;
        
        ns_per_tick = ns_elapsed / cycles_elapsed;
    }
};