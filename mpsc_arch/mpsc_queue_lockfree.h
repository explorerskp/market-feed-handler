#pragma once
#include <atomic>
#include <vector>
#include <cstddef>
#include <thread>




template<typename T, size_t Capacity>
class LockFreeMPSC {
private:
    struct Element {
        std::atomic<size_t> sequence;
        T data;
    };

    
    
    Element* buffer;
    
    
    alignas(64) std::atomic<size_t> head; 
    alignas(64) std::atomic<size_t> tail; 

public:
    LockFreeMPSC() {
        buffer = new Element[Capacity];
        for (size_t i = 0; i < Capacity; ++i) {
            
            
            buffer[i].sequence.store(i, std::memory_order_relaxed);
        }
        head.store(0, std::memory_order_relaxed);
        tail.store(0, std::memory_order_relaxed);
    }

    ~LockFreeMPSC() {
        delete[] buffer;
    }

    bool push(const T& item) {
        size_t t, idx;
        
        
        
        while (true) {
            t = tail.load(std::memory_order_relaxed);
            idx = t & (Capacity - 1); 
            
            
            size_t seq = buffer[idx].sequence.load(std::memory_order_acquire);
            size_t dif = (intptr_t)seq - (intptr_t)t;

            if (dif == 0) {
                
                if (tail.compare_exchange_weak(t, t + 1, std::memory_order_relaxed)) {
                    break; 
                }
            } else if (dif < 0) {
                
                
                return false; 
            } else {
                
            }
        }

        
        buffer[idx].data = item;
        
        
        
        buffer[idx].sequence.store(t + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t h = head.load(std::memory_order_relaxed);
        size_t idx = h & (Capacity - 1);

        
        size_t seq = buffer[idx].sequence.load(std::memory_order_acquire);
        size_t dif = (intptr_t)seq - (intptr_t)(h + 1);

        if (dif == 0) {
            
            item = buffer[idx].data;
            
            
            
            buffer[idx].sequence.store(h + Capacity, std::memory_order_release);
            
            
            head.store(h + 1, std::memory_order_relaxed);
            return true;
        }
        
        return false; 
    }
};