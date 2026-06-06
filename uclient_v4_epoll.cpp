// udp client// 06.06.26// ZeroK

/* NOTES:
 * made the client interactive by adding input thru fgets
 * removed threads - single threaded ops now
 * introduced epoll() - will continually check list of active fds only
 * used connected udp sockets
 * epoll remembers the watched file descriptors inside the kernel
 * while select requires rebuilding and passing the fd sets every call
 * epoll returns only the ready descriptors rather than forcing scans over the monitored range
 * */


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/epoll.h>
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
constexpr int  MAX_EVENTS   { 8 };                                                


volatile sig_atomic_t RUNNING { 1 };
void sig_handler (int sig) { RUNNING = 0; }


int main () {

    std::printf("\n\n=== UDP Client v4.0 (with epoll()) ===\n\n");

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

    // create epoll instance
    int epfd = epoll_create1 (0);
    if (epfd == -1) {
        perror ("epoll_create1");
        return EXIT_FAILURE;
    }

    // 1. register STDIN
    epoll_event ev {};

    ev.events    =  EPOLLIN;
    ev.data.fd   =  STDIN_FILENO;

    // add to interest list
    epoll_ctl (epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev);

    
    // 2. register udp socket
    ev.events    =   EPOLLIN;
    ev.data.fd   =   socketfd;

    // add to interest list
    epoll_ctl (epfd, EPOLL_CTL_ADD, socketfd, &ev);


    // 3. event buffer
    epoll_event events [MAX_EVENTS];
    
    char send_buffer [MAX_SIZE];
    char recv_buffer [MAX_SIZE];

    while (RUNNING) {
            int nfds = epoll_wait (epfd, events, MAX_EVENTS, -1);       // -1 = block until atleast 1 fd is ready
            if (nfds == -1) {
                if (errno == EINTR) break;
                perror ("epoll_pwait");
                break;
            }                                                                                    

            for (auto i {0}; i < nfds; ++i) {
                int fd = events[i].data.fd;

                // 4. process ready fd
                if (fd == STDIN_FILENO) {
            
                    if (!fgets (send_buffer, sizeof(send_buffer), stdin)) {
                        RUNNING = 0;
                        shutdown (socketfd, SHUT_RDWR);
                        break;
                    }


                    if (strlen(send_buffer) > 0) {
                        int sd = send (socketfd, send_buffer, strlen(send_buffer), 0);
                        if (sd == -1) perror ("send");
                        else std::printf("Sent %d bytes\n", sd);
                    }

                } // if (fd ==...)
        
    /*******************************************************************************************************/

                // 5. process ready fd
                else if (fd == socketfd) {

                    int bytes = recv (socketfd, recv_buffer, sizeof(recv_buffer)-1, 0);
                    if (bytes > 0) {
                        // write (STDOUT_FILENO, recv_buffer, bytes);
                        recv_buffer[bytes] = '\0';
                        std::printf("\rServer : %s\n", recv_buffer);    // \r to overwrite i/p line
                        fflush (stdout);                                // fflush to immediate o/p
                    }

                    else if (bytes == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                        perror ("recv"); 
                        break; 
                    }
            
                } // if (fd ==...)
            
            } // for 

        } // while

    /*******************************************************************************************************/

    // 5. close
    close (epfd);
    close (socketfd);

    std::printf("\n\n=== Client terminated ===\n");


    std::printf("\n\n");
    return EXIT_SUCCESS;
}
