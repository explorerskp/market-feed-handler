#pragma once
#include <vector>
#include <atomic>
#include <thread>
#include <fstream>
#include <iostream>

struct LogEntry {
    uint64_t timestamp;
    double price;
    long latency_ns;
};

class AsyncLogger {
    static const size_t BUFFER_SIZE = 1024 * 1024; 
    std::vector<LogEntry> ring_buffer;
    
    alignas(64) std::atomic<size_t> write_idx{0};
    alignas(64) std::atomic<size_t> read_idx{0};
    
    std::atomic<bool> running{true};
    std::thread background_thread;

public:
    AsyncLogger() : ring_buffer(BUFFER_SIZE) {
        background_thread = std::thread(&AsyncLogger::worker, this);
    }

    ~AsyncLogger() {
        running = false;
        if (background_thread.joinable()) background_thread.join();
    }

    void log(uint64_t ts, double price, long lat) {
        size_t curr_write = write_idx.load(std::memory_order_relaxed);
        size_t next_write = (curr_write + 1) & (BUFFER_SIZE - 1);

        if (next_write == read_idx.load(std::memory_order_acquire)) return; 

        ring_buffer[curr_write] = {ts, price, lat};
        write_idx.store(next_write, std::memory_order_release);
    }

private:
    void worker() {
        std::ofstream log_file("latency_metrics.csv");
        log_file << "Timestamp,Price,Latency_ns\n";

        
        while (running || read_idx.load(std::memory_order_acquire) != write_idx.load(std::memory_order_acquire)) {
            size_t curr_read = read_idx.load(std::memory_order_relaxed);
            
            if (curr_read == write_idx.load(std::memory_order_acquire)) {
                if (!running) break;
                std::this_thread::yield(); 
                continue;
            }

            LogEntry& e = ring_buffer[curr_read];
            log_file << e.timestamp << "," << e.price << "," << e.latency_ns << "\n";

            size_t next_read = (curr_read + 1) & (BUFFER_SIZE - 1);
            read_idx.store(next_read, std::memory_order_release);
        }
    }
};