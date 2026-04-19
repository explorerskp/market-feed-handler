#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <atomic>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>


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
    
    std::cout << "Loaded " << count << " packets (" << size / 1024 / 1024 << " MB) into RAM.\n";
}

void sender_thread(int id) {
    int sockfd;
    struct sockaddr_in servaddr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        return;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(TARGET_PORT);
    inet_pton(AF_INET, TARGET_IP, &servaddr.sin_addr);

    
    int sndbuf = 4 * 1024 * 1024; 
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    std::cout << "Sender " << id << " blasting..." << std::endl;

    size_t idx = 0;
    size_t total_size = market_data.size();
    
    
    while (keep_sending) {
        if (idx >= total_size) idx = 0;

        sendto(sockfd, &market_data[idx], sizeof(MarketPacket), 0, 
               (const struct sockaddr *)&servaddr, sizeof(servaddr));
        
        idx++;
    }
    close(sockfd);
}

int main() {
    load_data("../multi_data.bin");

    std::vector<std::thread> threads;
    std::cout << "Starting " << NUM_SENDERS << " high-speed sender threads...\n";
    
    for(int i=0; i<NUM_SENDERS; i++) {
        threads.emplace_back(sender_thread, i);
    }

    std::cout << "Running simulation for 30 seconds... (Press Ctrl+C to stop earlier)\n";
    
    
    std::this_thread::sleep_for(std::chrono::seconds(30));
    
    
    std::cout << "Stopping threads..." << std::endl;
    keep_sending = false; 

    for(auto& t : threads) {
        if(t.joinable()) t.join(); 
    }
    
    std::cout << "Simulation finished cleanly." << std::endl;
    return 0;
}