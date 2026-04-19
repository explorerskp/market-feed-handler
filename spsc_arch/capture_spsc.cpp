#include <iostream>
#include <fstream>
#include <vector>
#include <atomic>
#include <thread>
#include <iomanip>
#include <cstring>
#include <csignal>

#include "../udp_receiver.h"


#pragma pack(push, 1)
struct MarketPacket {
    uint64_t timestamp;
    char type;
    char side;
    double price;
    double quantity;
};
#pragma pack(pop)

const int PORT = 5000;
const int CAPTURE_LIMIT = 1000;

std::atomic<bool> keep_running{true};

void signal_handler(int signal) {
    keep_running = false;
}

int main() {

    std::signal(SIGINT, signal_handler);

    std::cout << "Starting SPSC Capture Tool..." << std::endl;
    std::cout << "Listening on Port " << PORT << ". Waiting for data..." << std::endl;

    try {

        UDPReceiver receiver(PORT);


        std::ofstream outfile("spsc_arch/received_dump.csv");
        if (!outfile.is_open()) {
            std::cerr << "Error: Could not create spsc_arch/received_dump.csv" << std::endl;
            return 1;
        }


        outfile << "PacketID,Timestamp,Type,Side,Price,Quantity\n";

        MarketPacket pkt;
        int count = 0;

        while (keep_running && count < CAPTURE_LIMIT) {
            if (receiver.receive(pkt)) {

                outfile << count << ","
                        << pkt.timestamp << ","
                        << pkt.type << ","
                        << pkt.side << ","
                        << std::fixed << std::setprecision(8) << pkt.price << ","
                        << std::fixed << std::setprecision(8) << pkt.quantity << "\n";

                count++;
                if (count % 100 == 0) {
                    std::cout << "Captured " << count << " packets..." << "\r" << std::flush;
                }
            }
        }

        std::cout << "\n\nSuccess! Captured " << count << " packets." << std::endl;
        std::cout << "Data saved to: spsc_arch/received_dump.csv" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}