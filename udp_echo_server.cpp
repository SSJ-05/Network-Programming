// UDP echo server// 30.05.26// ZeroK

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h>

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cerrno>


constexpr char SERVERPORT[] { "7777" };         // server's port client will be connecting to

// for graceful shutdowns
volatile sig_atomic_t RUNNING { 1 };
void sig_handler (int sig) { RUNNING = 0; }


int main () {

    std::printf("\n\n=== UDP Echo Server ===\n\n");

    // signals for graceful shutdowns
    signal (SIGINT, sig_handler);
    signal (SIGTERM, sig_handler);

    sigset_t emptyset;
    sigemptyset (&emptyset);



    // build server addr
    addrinfo hints {}, *res;
    hints.ai_family     =   AF_INET;
    hints.ai_socktype   =   SOCK_DGRAM;     // UDP
    hints.ai_flags      =   AI_PASSIVE;     // for binding

    int gai = getaddrinfo (nullptr, SERVERPORT, &hints, &res);

    if (gai != 0) {
        std::fprintf (stderr, "getaddrinfo: %s\n", gai_strerror(gai));  // getaddrinfo dont use errno/perror
        return EXIT_FAILURE;
    }


    // 1. create udp socket
    int sockfd = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror ("socket");
        exit (EXIT_FAILURE);
    }
    
    // // 1a. make it non blocking
    int flag = fcntl (sockfd, F_GETFL, 0);
    fcntl (sockfd, F_SETFL, flag | O_NONBLOCK);
    
    // 1b. port reuse
    int yes = 1;
    setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    // 2. bind
    int bd = bind (sockfd, res->ai_addr, res->ai_addrlen);
    if (bd == -1) {
        perror ("bind");
        return EXIT_FAILURE;
    }

    freeaddrinfo (res);

    std::printf("Waiting for packets on port %s.....\n\n", SERVERPORT);


    // 3. main event loop
    char buff [1024];    
    
    while (RUNNING) {
        sockaddr_storage    cli_addr    {};
        socklen_t           cli_addrlen { sizeof(cli_addr) };

        // print sender ip addr
        char ipstr [INET_ADDRSTRLEN];
        sockaddr_in* ipv4 = (sockaddr_in*)&cli_addr;


        // 4. sendto / recvfrom
        int bytes = recvfrom (sockfd, buff, sizeof(buff), 0, (sockaddr*)&cli_addr, &cli_addrlen); 
        if (bytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror ("recvfrom");
            break;
        }

        if (bytes > 0) {
            inet_ntop (AF_INET, &ipv4->sin_addr, ipstr, sizeof(ipstr));
            std::printf("\nPacket received from %s:%d\n", ipstr, ntohs(ipv4->sin_port));

            std::printf("Received %d bytes\n", bytes);
            write (STDOUT_FILENO, buff, bytes);

            // echo back to same client who sent it
            int sent = sendto (sockfd, buff, bytes, 0, (sockaddr*)&cli_addr, cli_addrlen);    // pass cli_addrlen by value

            if (sent == -1) perror ("sendto");
            else std::printf("Echoed back %d bytes\n", sent);
        }
    } // while closed


    // 5. close
    close (sockfd);

    std::printf("\n\n=== Server closed ===\n");

    std::printf("\n\n");
    return EXIT_SUCCESS;
}
