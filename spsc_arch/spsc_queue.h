#pragma once
#include <atomic>
#include <cstddef>

template<typename T, size_t Capacity>
class SPSCQueue {
private:
    struct alignas(64) Slot {
        T value;
    };
    Slot buffer[Capacity];
    alignas(64) std::atomic<size_t> head = {0}; 
    alignas(64) std::atomic<size_t> tail = {0}; 

public:
    bool push(const T& item) {
        size_t t = tail.load(std::memory_order_relaxed);
        size_t next_t = (t + 1) % Capacity;
        if (next_t == head.load(std::memory_order_acquire)) return false;
        buffer[t].value = item;
        tail.store(next_t, std::memory_order_release); 
        return true;
    }

    bool pop(T& item) {
        size_t h = head.load(std::memory_order_relaxed);
        if (h == tail.load(std::memory_order_acquire)) return false;
        item = buffer[h].value;
        head.store((h + 1) % Capacity, std::memory_order_release); 
        return true;
    }
};