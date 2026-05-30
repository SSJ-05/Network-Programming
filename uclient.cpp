// udp client// 30.05.26// ZeroK

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cerrno>


constexpr char SERVERPORT[] { "7777" };         // server's port client will be connecting to
constexpr char SERVADDR[]   { "127.0.0.1" };

int main () {

    std::printf("\n\n=== UDP Client ===\n\n");

    // build addr struct
    addrinfo hints {}, *res;
    hints.ai_family     =   AF_INET;
    hints.ai_socktype   =   SOCK_DGRAM;
    
    getaddrinfo (SERVADDR, SERVERPORT, &hints, &res);

    // 1. create a socket
    int socketfd = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
    if (socketfd == -1) {
        perror ("socket");
        exit (EXIT_FAILURE);
    }

    // 2. connect
    // int result = connect ();

    // 3. send message to UDP server
    constexpr const char* msg { "Client says Hello world.\n" };

    int sn = sendto (socketfd, msg, strlen(msg), MSG_DONTWAIT, res->ai_addr, res->ai_addrlen);
    if (sn == -1) {
        perror ("sendto");
        return EXIT_FAILURE;
    }


    // 4. recv msg from UDP server
    // std::uint8_t buffer [1024];
    //
    // int r;
    // while ((r = recvfrom (socketfd, buffer, sizeof(buffer), 0, res->ai_addr, res->ai_addrlen)) > 0) {
    //     write (STDOUT_FILENO, buffer, n);
    // }


    // 5. close
    freeaddrinfo (res);
    close (socketfd);

    // std::printf("actual bytes: %d\n", strlen(msg));
    std::printf("Client sent %d bytes\n", sn);
    std::printf("Client at disconnected from fd %d\n", socketfd);

    std::printf("\n\n");
    return EXIT_SUCCESS;
}
