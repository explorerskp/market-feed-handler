#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <sched.h>
#include <pthread.h>

#include "../udp_receiver.h"
#include "../common.h"
#include "../logger.h"     
#include "../tsc_clock.h"  
#include "spsc_queue.h"


const int PORT = 5000;
const int NUM_THREADS = 2; 
const int QUEUE_SIZE = 16384; 


struct Envelope {
    MarketUpdate data;
    uint64_t arrival_tsc; 
};


SPSCQueue<Envelope, QUEUE_SIZE> queues[NUM_THREADS];
std::atomic<bool> running{true};
std::atomic<long> backpressure_spins{0}; 
TSCClock clock_service; 


void pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}


void network_thread(int id, int core_id) {
    pin_to_core(core_id);
    UDPReceiver receiver(PORT);
    RawUpdate raw;
    
    while (running) {
        if (receiver.receive(raw)) {
            
            uint64_t tsc_now = TSCClock::now();

            
            Envelope env;
            env.data = MarketUpdate::from_raw(raw);
            env.arrival_tsc = tsc_now;

            
            while (!queues[id].push(env)) {
                backpressure_spins.fetch_add(1, std::memory_order_relaxed);
                if (!running) break;
                std::this_thread::yield();
            }
        }
    }
}


void strategy_thread(int core_id) {
    pin_to_core(core_id);
    
    AsyncLogger logger; 
    std::cout << "[Strategy] SPSC Engine Started. Logging metrics..." << std::endl;

    long long total_pkts = 0;
    long long stale_pkts = 0;
    uint64_t max_ts_seen = 0;

    auto start_time = std::chrono::high_resolution_clock::now();

    while (running) {
        bool work_done = false;

        for (int i = 0; i < NUM_THREADS; i++) {
            Envelope env;
            if (queues[i].pop(env)) {
                work_done = true;
                total_pkts++;
                
                
                uint64_t now_tsc = TSCClock::now();
                long latency_ns = clock_service.to_ns(now_tsc - env.arrival_tsc);

                
                if (env.data.timestamp < max_ts_seen) {
                    stale_pkts++;
                } else {
                    max_ts_seen = env.data.timestamp;
                }

                
                
                
                
                logger.log(env.data.timestamp, env.data.price, latency_ns);

                
                if (total_pkts % 500000 == 0) {
                     auto now = std::chrono::high_resolution_clock::now();
                     double elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
                     if(elapsed > 0) {
                         std::cout << "\rProcessed: " << total_pkts 
                                   << " | Rate: " << (long)(total_pkts/elapsed) << "/s"
                                   << " | Staleness: " << std::fixed << std::setprecision(2) 
                                   << (stale_pkts * 100.0 / total_pkts) << "% " 
                                   << " | Latency: " << latency_ns << "ns "
                                   << " | Spins: " << backpressure_spins.load()
                                   << std::flush;
                     }
                }
            }
        }
        
        if (!work_done) {
             std::this_thread::yield(); 
        }
    }
}

int main() {
    std::thread t1(network_thread, 0, 0);
    std::thread t2(network_thread, 1, 2);
    std::thread t3(strategy_thread, 1);

    std::cout << "Press ENTER to stop.\n";
    std::cin.get();
    running = false;

    t1.join();
    t2.join();
    t3.join();
    return 0;
}