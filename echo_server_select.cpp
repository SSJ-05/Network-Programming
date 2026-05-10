// select/poll server// 10.05.26// ZeroK

// ***** Select() Server *****
// event driven concurrency, I/O multiplexing
// no fork, no threads, one process handles multiple clients/watches all sockets
// workflow: listener = accept new connection, master socket (listening socket) = tracks all active sockest
// select() = if master socket is ready -> accept new client, if client socket is ready -> send/recv
// FD_ISSET = check if socket i is ready
// imp: select() blocks until socket is readable/writable, new client arrives, error occurs 
// cons: repeatedly scan all sockets for availability in O(n), scales poorly
// select() modifies fd_set, copying master in every iteration is needed


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>

constexpr char MYPORT[] { "5555" };
constexpr int  MAX_CLI  { 32 };

int main() {
    std::printf("\n\n=== Select() Echo Server ===\n\n");


    // getaddrinfo
    addrinfo hints {}, *res;

    hints.ai_family     =   AF_INET;
    hints.ai_socktype   =   SOCK_STREAM;
    hints.ai_flags      =   AI_PASSIVE;

    getaddrinfo (nullptr, MYPORT, &hints, &res);    // dynamically allocate linked lists



    // socket
    int listener = socket (res->ai_family, res->ai_socktype, res->ai_protocol);

    int yes { 1 };
    setsockopt (listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    bind (listener, res->ai_addr, res->ai_addrlen);

    listen (listener, SOMAXCONN);

    freeaddrinfo (res);     // free linked lists/memory



    // fd sets
    fd_set master;      // store all active sockets, never modify
    fd_set read_fd;     // temp copy - will be modified by select()

    FD_ZERO (&master);
    FD_ZERO (&read_fd);

    // listening socket
    FD_SET (listener, &master);     // add socket to monitored set of sockets

    int fdmax { listener };

    std::printf("\nWaiting for connection on port %s...\n", MYPORT);



    // event loop
    while (true) {
        // select() modifies read_fd
        // need to cpy master in each iteration
        read_fd = master;

        // select modifies read_fd in place, keeps all active sockets, removes inactive sockets
        int activity = select (fdmax + 1, &read_fd, nullptr, nullptr, nullptr);

        if (activity == -1) {
            perror ("select");
            exit (1);
        }

        // scan all fd
        for (int i {0}; i <= fdmax; ++i) {
            if (!FD_ISSET (i, &read_fd)) continue;      // FD_ISSET = this socket is ready

                // new client connect
                if (i == listener) {            // new incoming connection, not client data
                    sockaddr_storage cli_addr {};
                    socklen_t addrlen { sizeof(cli_addr) };

                    int newfd = accept (listener, (sockaddr*)&cli_addr, &addrlen);

                    if (newfd == -1) {
                        perror ("accept");
                        continue;
                    }

                    // limit no. of clients
                    if (newfd >= FD_SETSIZE || newfd >= MAX_CLI) {
                        std::printf("\nToo many clients.\n");
                        close (newfd);
                        continue;
                    }

                    FD_SET (newfd, &master);

                    if (newfd > fdmax) fdmax = newfd;
                    
                    std::printf("New connection fd: %d\n", newfd);
                    
                }

                // recv data sent by existing client
                else {
                    char buffer [1024];
                    int bytes = recv (i, buffer, sizeof(buffer), 0);

                    // client disconnect
                    if (bytes <= 0) {
                        if (bytes == 0) std::printf("Socket %d disconnected.\n", i);
                        else perror("disconnect");

                        close (i);
                        FD_CLR (i, &master);
                    }
                
                    // echo back to client
                    else {
                        send (i, buffer, bytes, 0);
                        write (STDOUT_FILENO, buffer, bytes);                
                    }
               }
          }
    }

    close (listener);

    std::printf("\n\n");
    return EXIT_SUCCESS;
}
