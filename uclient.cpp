// udp client// 30.05.26// ZeroK

/* NOTES:
 * connect() in udp is diff from connect in tcp
 * connect() in tcp = create connection state (sequencing, retransmission, congestion control, flow control)
 * use when accuracy > latency = must recv in correct order
 * connect() in udp = only store destination addr (no handshake, ordering, flow control)
 * use when latency > reliability = must move forward w/o stallling the system
 * */

// IMP: use send/recv with connect and sendto/recvfrom w/o connect


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
constexpr char SERVADDR[]   { "127.0.0.1" };    // server's ip addr


int main () {

    std::printf("\n\n=== UDP Client ===\n\n");

    // build addr struct
    addrinfo hints {}, *res;
    hints.ai_family     =   AF_INET;
    hints.ai_socktype   =   SOCK_DGRAM;
    
    int rv = getaddrinfo (SERVADDR, SERVERPORT, &hints, &res);
    if (rv != 0) {
        std::fprintf (stderr, "getaddrinfo: %s\n", gai_strerror(rv));  // getaddrinfo dont use errno/perror
        return EXIT_FAILURE;
    }

    // 1. create a socket
    int socketfd = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
    if (socketfd == -1) {
        perror ("socket");
        exit (EXIT_FAILURE);
    }

    // 2. connect
    // int result = connect (socketfd, res->ai_addr, res->ai_addrlen);
    // if (result == -1) {
    //     perror ("connect");
    //     return EXIT_FAILURE;
    // }

    // 3. send message to UDP server
    constexpr char msg[] { "Client says Hello world.\n" };

    int sn = sendto (socketfd, msg, strlen(msg), 0, res->ai_addr, res->ai_addrlen);
    // int sn = send(socketfd, msg, strlen(msg), 0);
    if (sn == -1) {
        perror ("sendto");
        return EXIT_FAILURE;
    }
    // std::printf("Echo sent to server : %s\n", msg);


    // 4. recv msg from UDP server
    char buffer [1024];
    sockaddr_storage    serv_addr   {};
    socklen_t           servlen     { sizeof(serv_addr) };

    int bytes = recvfrom (socketfd, buffer, sizeof(buffer)-1, 0, (sockaddr*)&serv_addr, &servlen);
    // int bytes = recv(socketfd, buffer, sizeof(buffer)-1, 0);
    if (bytes == -1) perror ("recvfrom");
    
    if (bytes > 0) {
        // buffer [bytes] = '\0';
        // std::printf("Echo received from server : %s\n", buffer);
        write (STDOUT_FILENO, buffer, bytes);
    }


    // 5. close
    freeaddrinfo (res);
    close (socketfd);

    std::printf("Sent %d bytes to %s:%s\n", sn, SERVADDR, SERVERPORT);
    std::printf("Client disconnected from fd %d\n", socketfd);

    std::printf("\n\n");
    return EXIT_SUCCESS;
}
