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
#include "collision.h"         


const int PORT = 5000;
const int NUM_SYMBOLS = 5; 


std::vector<CollisionTree> trees(NUM_SYMBOLS);
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
            
            MarketUpdate update = MarketUpdate::from_raw(raw);
            int sym_id = update.timestamp % NUM_SYMBOLS;
            
            if (sym_id >= 0 && sym_id < NUM_SYMBOLS) {
                update.symbol_id = sym_id;
                
                
                
                
                trees[sym_id].push(update, tsc_now);
            }
        }
    }
}


void strategy_thread(int core_id) {
    pin_to_core(core_id);
    AsyncLogger logger;
    std::cout << "[Strategy] Multi-Tree Engine Started (5 Assets)..." << std::endl;

    long long total_reads = 0;
    long long stale_reads = 0;
    
    std::vector<uint64_t> last_read_ts(NUM_SYMBOLS, 0);
    std::vector<uint64_t> max_ts_seen(NUM_SYMBOLS, 0);

    auto start = std::chrono::high_resolution_clock::now();

    while (running) {
        bool did_work = false;

        for (int i = 0; i < NUM_SYMBOLS; i++) {
            MarketUpdate u;
            uint64_t arrival_tsc;
            
            
            if (trees[i].read_root(u, arrival_tsc)) {
                
                if (u.timestamp > last_read_ts[i]) {
                    did_work = true;
                    total_reads++;
                    last_read_ts[i] = u.timestamp; 

                    
                    if (u.timestamp < max_ts_seen[i]) {
                        stale_reads++;
                    } else {
                        max_ts_seen[i] = u.timestamp;
                    }
                    
                    
                    uint64_t now_tsc = TSCClock::now();
                    long latency_ns = clock_service.to_ns(now_tsc - arrival_tsc);
                    
                    
                    logger.log(u.timestamp, u.price, latency_ns); 
                }
            }
        }
        
        if (total_reads % 100000 == 0 && total_reads > 0) {
             auto now = std::chrono::high_resolution_clock::now();
             double elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
             if(elapsed > 0) {
                 std::cout << "\rProcessed: " << total_reads 
                           << " | Rate: " << (long)(total_reads/elapsed) << "/s"
                           << " | Staleness: " << std::fixed << std::setprecision(4) 
                           << (stale_reads * 100.0 / total_reads) << "% " 
                           << "| Spins: " << backpressure_spins.load()
                           << std::flush;
             }
        }

        if (!did_work) {
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