// Echo server// 06.05.26// ZeroK
// client sends 'hello' -> server sends back 'hello'
// server flow : socket -> bind -> listen -> accept -> send/recv

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>


int main () {
    std::printf("\n\n");

    constexpr char           MY_PORT[] { "5555" };   // client will connect to this port
    constexpr std::uint8_t   BACKLOG   { 10 };       // no. of requests to connect to be queued

    // create struct addrinfo
    addrinfo hints {}, *res;

    hints.ai_family     =   AF_INET;
    hints.ai_socktype   =   SOCK_STREAM;
    hints.ai_flags      =   AI_PASSIVE;

    getaddrinfo (nullptr, MY_PORT, &hints, &res);

    
    
    // 1. create a socket
    int socketfd = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
    
    std::uint8_t yes { 1 };
    setsockopt (socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    
    
    // 2. bind to port we passed in getaddrinfo
    bind (socketfd, res->ai_addr, res->ai_addrlen);
    


    // 3. listen for incoming requests from client
    listen (socketfd, BACKLOG);
    

    // 4. accept request - TCP handshake


    // 5. send


    // 6. recv




    std::printf("\n\n");
    return EXIT_SUCCESS;
}
