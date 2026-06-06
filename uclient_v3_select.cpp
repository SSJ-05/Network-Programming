// udp client// 06.06.26// ZeroK

/* NOTES:
 * made the client interactive by adding input thru fgets
 * removed threads - single threaded ops now
 * introduced select() - will continually check list of fds
 * used connected udp sockets
 * */


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/select.h>
#include <signal.h>

#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <algorithm>


constexpr char SERVERPORT[] { "7777" };         // server's port client will be connecting to
constexpr char SERVADDR[]   { "127.0.0.1" };    // server's ip addr
constexpr int  MAX_SIZE     { 1024 };           // buffer size 


volatile sig_atomic_t RUNNING { 1 };
void sig_handler (int sig) { RUNNING = 0; }


int main () {

    std::printf("\n\n=== UDP Client v3.0 (with select()) ===\n\n");

    signal (SIGINT,  sig_handler);
    signal (SIGTERM, sig_handler);



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


    // 2. connect
    int result = connect (socketfd, res->ai_addr, res->ai_addrlen);
    if (result == -1) {
        perror ("connect");
        freeaddrinfo (res);
        close (socketfd);
        return EXIT_FAILURE;
    }
    
    freeaddrinfo (res);

    /*******************************************************************************************************/

    fd_set master;
    fd_set read_fd;

    FD_ZERO (&master);
    FD_ZERO (&read_fd);

    FD_SET (STDIN_FILENO, &master);
    FD_SET (socketfd, &master);
    int fdmax = std::max (STDIN_FILENO, socketfd);


    char send_buffer [MAX_SIZE];
    char recv_buffer [MAX_SIZE];

    while (RUNNING) {
            read_fd = master;
            int activity = select (fdmax + 1, &read_fd, nullptr, nullptr, nullptr);
            if (activity == -1) {
                if (errno == EINTR) break;      // catch signal on force exit
                perror ("select");              // actual error
                break;
            }

            if (FD_ISSET (STDIN_FILENO, &read_fd)) {
               if (!fgets (send_buffer, sizeof(send_buffer), stdin)) {
                    RUNNING = false;
                    shutdown (socketfd, SHUT_RDWR);
                    break;
                }


                if (strlen(send_buffer) > 0) {
                    int sd = send (socketfd, send_buffer, strlen(send_buffer), 0);
                    if (sd == -1) perror ("sendto");
                    else std::printf("Sent %d bytes\n", sd);
                }

            } // if (FD_ISSET...)
        
    /*******************************************************************************************************/

            if (FD_ISSET (socketfd, &read_fd)) {
                int bytes = recv (socketfd, recv_buffer, sizeof(recv_buffer)-1, 0);
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
            
            } // if (FD_ISSET...)

        } // while

    /*******************************************************************************************************/

    // 5. close
    close (socketfd);

    std::printf("\n\n=== Client terminated ===\n");


    std::printf("\n\n");
    return EXIT_SUCCESS;
}
