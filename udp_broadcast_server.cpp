// UDP Broadcast server// 31.05.26// ZeroK

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
constexpr int  MAX_SIZE     { 1024 };           // buffer size                                     
constexpr int  MAX_CLI      { 64 };             // no. of clients



// client struct
struct alignas(64) Client {
    sockaddr_storage    addr     {};
    socklen_t           addrlen  {};
};

alignas(64) Client clients [MAX_CLI];
alignas(64) std::size_t cli_count {};


// client checker func
bool is_same_client (const sockaddr_storage &a, const sockaddr_storage &b) 
{
    auto* aa = (const sockaddr_in*)&a;
    auto* bb = (const sockaddr_in*)&b;

    return (aa->sin_addr.s_addr == bb->sin_addr.s_addr) 
        && (aa->sin_port == bb->sin_port);
}

// for graceful shutdown
volatile sig_atomic_t RUNNING { 1 };
void sig_handler (int sig) { RUNNING = 0; }



int main () {

    std::printf("\n\n=== UDP Broadcast Server ===\n\n");

    // signals
    signal (SIGINT, sig_handler);
    signal (SIGTERM, sig_handler);

    sigset_t emptyset;
    sigemptyset (&emptyset);



    // build server addr
    addrinfo hints {}, *res;
    hints.ai_family     =   AF_INET;        // ipv4
    hints.ai_socktype   =   SOCK_DGRAM;     // UDP
    hints.ai_flags      =   AI_PASSIVE;     // for binding

    int rv = getaddrinfo (nullptr, SERVERPORT, &hints, &res);

    if (rv != 0) {
        std::fprintf (stderr, "getaddrinfo: %s\n", gai_strerror(rv));  // getaddrinfo dont use errno/perror
        return EXIT_FAILURE;
    }


    // 1. create udp socket
    int sockfd = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror ("socket");
        exit (EXIT_FAILURE);
    }
    
    // 1a. make it non blocking
    int flag = fcntl (sockfd, F_GETFL, 0);
    fcntl (sockfd, F_SETFL, flag | O_NONBLOCK);
    
    // 1b. port reuse
    constexpr int yes { 1 };
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
    // set kernel side send and recv buffer to 256 KB
    constexpr int buf_sz { 256 * 1024 };
    setsockopt (socketfd, SOL_SOCKET, SO_SNDBUF, &buf_sz, sizeof(buf_sz));  
    setsockopt (socketfd, SOL_SOCKET, SO_RCVBUF, &buf_sz, sizeof(buf_sz));
 
    char buff [MAX_SIZE];    
    
    while (RUNNING) {
        sockaddr_storage    cli_addr    {};
        socklen_t           cli_addrlen { sizeof(cli_addr) };


        int bytes = recvfrom (sockfd, buff, sizeof(buff), 0, (sockaddr*)&cli_addr, &cli_addrlen); 
        if (bytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;      // spinning, no sleep
            perror ("recvfrom");
            break;
        }

        // if unknown client, add to array
        bool known_client { false };

        for (auto i {0uz}; i < cli_count; ++i) {
            const Client& c = clients[i];

            if (is_same_client (c.addr, cli_addr)) {
                known_client = true;
                break;
            }
        }

        if (!known_client) {
            clients[cli_count++] = { cli_addr, cli_addrlen };

            std::printf("\nNew client registered\n" 
                        "Total clients : %zu\n", cli_count);
        }

        // print sender ip addr
        char ipstr [INET_ADDRSTRLEN];
        sockaddr_in* ipv4 = (sockaddr_in*)&cli_addr;


        if (bytes > 0) {
            inet_ntop (AF_INET, &ipv4->sin_addr, ipstr, sizeof(ipstr));
            std::printf("Packet received from %s:%d\n", ipstr, ntohs(ipv4->sin_port));

            std::printf("Received %d bytes\n", bytes);
            write (STDOUT_FILENO, buff, bytes);

            // broadcast to all clients
            for (auto i {0uz}; i < cli_count; ++i) {
                const Client& c = clients[i];
                int sent = sendto (sockfd, buff, bytes, 0, (sockaddr*)&c.addr, c.addrlen);    // pass c.addrlen by value
            
                if (sent == -1) perror ("sendto");
                else std::printf("\nEchoed back %d bytes to client %d\n", sent, i+1);
            }
        }

    } // while closed


    // 5. close
    close (sockfd);

    std::printf("\n\n=== Server closed ===\n");

    std::printf("\n\n");
    return EXIT_SUCCESS;
}
