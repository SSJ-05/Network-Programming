// epoll echo server// 10.05.26// ZeroK

// hows its diff from select/poll server?
// select/poll scans all sockets - even inactive ones in O(n) - wasteful
// epoll - kernel maintains a ready list of active sockets
// 3 syscalls - epoll_create1 = create a kernel event manager
// epoll_ctl = register/remove sockets
// epoll_wait = ask kernel for ready sockets

// level triggered = unread data remains active, epoll continues notifying
// edge triggered  = 


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <unistd.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>

constexpr char MYPORT[]     { "5555" };
constexpr int  MAX_CLI      { 32 };
constexpr int  BUFFER_SIZE  { 1024 };


int main() {
    std::printf("\n=== Level Triggered epoll server ===\n");
    
    // getaddrinfo
    addrinfo hints {}, *res;
    
    hints.ai_family     =   AF_INET;
    hints.ai_socktype   =   SOCK_STREAM;
    hints.ai_flags      =   AI_PASSIVE;

    getaddrinfo (nullptr, MYPORT, &hints, &res);


    
    // socket
    int listener = socket (res->ai_family, res->ai_socktype, res->ai_protocol);

    int yes { 1 };
    setsockopt (listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    bind (listener, res->ai_addr, res->ai_addrlen);

    listen (listener, SOMAXCONN);

    freeaddrinfo (res);



    // create epoll instance
    int epfd { epoll_create1(0) };
    
    if (epfd == -1) {
        perror ("epoll_create1");
        exit(1);
    }



    // register listener socket
    epoll_event ev {};
    epoll_event events [MAX_CLI];

    ev.events   = EPOLLIN;
    ev.data.fd  = listener;

    if (epoll_ctl (epfd, EPOLL_CTL_ADD, listener, &ev) == -1) {
        perror ("epoll_ctl");
        exit(1);
    }

    std::printf("\nWaiting for connection...\n");



    // event loop
    while (1) {
        int nfds = epoll_wait (epfd, events, MAX_CLI, -1);

        if (nfds == -1) {
            perror ("epoll_wait");
            exit(1);
        }

        // process only active sockets
        for (int i {0}; i < nfds; ++i) {
            int currentfd = events[i].data.fd;

            // connect new client
            if (currentfd == listener) {
                sockaddr  cli_addr  {};
                socklen_t addrlen   { sizeof(cli_addr) };

                int newfd = accept (listener, (sockaddr*)&cli_addr, &addrlen);

                if (newfd == -1) {
                    perror("accept");
                    continue;
                }

                std::printf("New connection fd: %d\n", newfd);

                epoll_event cli_ev {};
                cli_ev.events   = EPOLLIN;
                cli_ev.data.fd  = newfd;

                epoll_ctl (epfd, EPOLL_CTL_ADD, newfd, &cli_ev);
            }

            // recv existing client data
            else {
                char buffer [1024];
                int bytes = recv (currentfd, buffer, sizeof(buffer), 0);

                if (bytes <= 0) {
                    if (bytes == 0) std::printf("Socket %d disconnected.\n", currentfd);
                    else perror("recv");

                    close (currentfd);
                    epoll_ctl (epfd, EPOLL_CTL_DEL, currentfd, nullptr);
                }
                // echo back
                else {
                    send (currentfd, buffer, bytes, 0);
                    write (STDOUT_FILENO, buffer, bytes);
                }
            }
        }
    }

    close (listener);
    close (epfd);


    std::printf("\n\n");
    return EXIT_SUCCESS;
}

