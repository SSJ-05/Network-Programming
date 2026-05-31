// udp client// 31.05.26// ZeroK

/* NOTES:
 * made the client interactive by adding input thru fgets
 * sendto and recvfrom now operate on different threads
 * */


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

#include <thread>
#include <atomic>

constexpr char SERVERPORT[] { "7777" };         // server's port client will be connecting to
constexpr char SERVADDR[]   { "127.0.0.1" };    // server's ip addr
constexpr int  MAX_SIZE     { 1024 };                                                

std::atomic<bool> RUNNING { true };


int main () {

    std::printf("\n\n=== UDP Client v2.0 (with threads) ===\n\n");

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
        freeaddrinfo (res);
        exit (EXIT_FAILURE);
    }
    std::printf("Client registered at fd %d\n", socketfd);
    std::printf("Type ':q' to exit\n", socketfd);

    // 2. connect
    // int result = connect (socketfd, res->ai_addr, res->ai_addrlen);
    // if (result == -1) {
    //     perror ("connect");
    //     return EXIT_FAILURE;
    // }

    constexpr int buf_sz { 256 * 1024 };
    setsockopt (socketfd, SOL_SOCKET, SO_SNDBUF, &buf_sz, sizeof(buf_sz));
    setsockopt (socketfd, SOL_SOCKET, SO_RCVBUF, &buf_sz, sizeof(buf_sz));
    
    /*******************************************************************************************************/

    // sender on thread 1
    std::thread sender ( [&] () noexcept {
        char send_buffer [MAX_SIZE];

        while (RUNNING) {
            if (!fgets (send_buffer, sizeof(send_buffer), stdin)) {
                RUNNING = false;
                shutdown (socketfd, SHUT_RDWR);
                break;
            }

            // remove '\n' from fgets
            std::size_t len = strlen (send_buffer);
            if (len > 0 && send_buffer[len-1] == '\n') {
                send_buffer[len-1] = '\0';
                len--;
            }

            // check for quit 
            if (strncmp (send_buffer, ":q", 2) == 0) {
                RUNNING = false;
                shutdown (socketfd, SHUT_RDWR);
                return;
            }

            if (len > 0) { 
                int sd = sendto (socketfd, send_buffer, len, MSG_DONTWAIT, res->ai_addr, res->ai_addrlen);
                if (sd == -1) perror ("sendto");
                else std::printf("Sent %d bytes\n", sd);
            }
        }
    } );
        
    /*******************************************************************************************************/

    // receiver on thread 2
    std::thread receiver ( [&] () noexcept {
        sockaddr_storage    serv_addr   {};
        socklen_t           servlen     { sizeof(serv_addr) };

        char recv_buffer [MAX_SIZE];
        
        while (RUNNING) {
            int bytes = recvfrom (socketfd, recv_buffer, sizeof(recv_buffer)-1, MSG_DONTWAIT, (sockaddr*)&serv_addr, &servlen);
            if (bytes > 0) {
                // write (STDOUT_FILENO, recv_buffer, bytes);
                recv_buffer[bytes] = '\0';
                std::printf("\rServer : %s\n", recv_buffer);    // \r to overwrite i/p line
                fflush (stdout);                                // fflush to immediate o/p
            }

            else if (bytes == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                perror ("recvfrom"); 
                break; 
            }
            
            else if (bytes == 0) {
                RUNNING = false;
                break; 
            }
        }
    } );

    /*******************************************************************************************************/

    sender.join(); 
    receiver.join();


    // 5. close
    freeaddrinfo (res);
    close (socketfd);

    std::printf("Client terminated.\n");


    std::printf("\n\n");
    return EXIT_SUCCESS;
}
