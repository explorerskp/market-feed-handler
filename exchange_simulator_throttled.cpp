#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <atomic>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>


const char* TARGET_IP = "127.0.0.1";
const int TARGET_PORT = 5000;
const int NUM_SENDERS = 4; 


std::atomic<bool> keep_sending{true};

#pragma pack(push, 1)
struct MarketPacket {
    uint64_t timestamp; 
    char type;          
    char side;          
    double price;       
    double quantity;    
};
#pragma pack(pop)

std::vector<MarketPacket> market_data;


void load_data(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error: Cannot open " << filename << std::endl;
        exit(1);
    }
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    size_t count = size / sizeof(MarketPacket);
    market_data.resize(count);
    file.read(reinterpret_cast<char*>(market_data.data()), size);
    
    std::cout << "Loaded " << count << " packets.\n";
}


void sender_thread(int id, int target_rate_per_thread) {
    int sockfd;
    struct sockaddr_in servaddr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) return;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(TARGET_PORT);
    inet_pton(AF_INET, TARGET_IP, &servaddr.sin_addr);

    
    double ns_per_packet = 1e9 / target_rate_per_thread;
    auto next_send_time = std::chrono::steady_clock::now();

    size_t idx = 0;
    size_t total_size = market_data.size();
    
    while (keep_sending) {
        if (idx >= total_size) idx = 0;

        
        auto now = std::chrono::steady_clock::now();
        if (now < next_send_time) {
            
            while (std::chrono::steady_clock::now() < next_send_time);
        }

        
        sendto(sockfd, &market_data[idx], sizeof(MarketPacket), 0, 
               (const struct sockaddr *)&servaddr, sizeof(servaddr));
        
        
        next_send_time += std::chrono::nanoseconds((long)ns_per_packet);
        idx++;
    }
    close(sockfd);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./exchange_throttled <rate_per_sec>\n";
        std::cerr << "Example: ./exchange_throttled 500000\n";
        return 1;
    }

    int total_rate = std::atoi(argv[1]);
    int rate_per_thread = total_rate / NUM_SENDERS;

    load_data("market_data.bin");

    std::vector<std::thread> threads;
    std::cout << "Starting " << NUM_SENDERS << " senders.";
    std::cout << " Total Rate: " << total_rate << " pkts/sec.\n";
    
    for(int i=0; i<NUM_SENDERS; i++) {
        threads.emplace_back(sender_thread, i, rate_per_thread);
    }

    
    std::this_thread::sleep_for(std::chrono::seconds(30));
    
    std::cout << "Stopping threads...\n";
    keep_sending = false;

    for(auto& t : threads) {
        if(t.joinable()) t.join();
    }
    
    return 0;
}