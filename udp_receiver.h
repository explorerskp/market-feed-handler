#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <stdexcept>

class UDPReceiver {
    int sockfd;
    struct sockaddr_in servaddr;

public:
    UDPReceiver(int port) {
        
        if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            throw std::runtime_error("Socket creation failed");
        }

        
        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            perror("setsockopt reuseport");
        }

        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = INADDR_ANY;
        servaddr.sin_port = htons(port);

        
        if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
            throw std::runtime_error("Bind failed");
        }
    }

    
    template<typename T>
    bool receive(T& buffer) {
        int n = recvfrom(sockfd, &buffer, sizeof(T), MSG_WAITALL, nullptr, nullptr);
        return n > 0;
    }

    ~UDPReceiver() {
        close(sockfd);
    }
};