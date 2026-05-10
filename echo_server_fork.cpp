// Echo server// 06.05.26// ZeroK

// ***** Iterative Server *****
// client sends 'hello' -> server sends back 'hello'
// client flow : socket() -> connect() -> send() / recv()
// server flow : socket -> bind -> listen -> accept -> send/recv
// imp: accept() creates a new socket


// ***** Multi-client Server *****
// workflow: parent accept() client -> fork() -> child handles client -> parent goes back to accept() ing new clients
// parent flow: listen for new connections -> accept() clients -> fork() clients to child -> reaps zombies
// child flow: handle one client -> exit
// imp: after fork() both processes inherit both listening and connected sockets
// each process (parent and child) should give up one socket to avoid resource leaks
// take care of zombie processes... (to be coded)
// fcntl creates non blocking new sockets - now send/recv dont block when sockets are idle

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string_view>


// waitpid(-1...) = reap zombie processes
// WNOHANG = dont block/wait when if no zombies
// multiple child processe might die,
// therefore, loop until all zombies are collected
void zombie_reaper (int sig, siginfo_t* info, void* context) {
    int status;
    pid_t pid;

    while ((pid = waitpid (-1, nullptr, WNOHANG)) > 0) {
        write (STDERR_FILENO, "Reaped child.\n", sizeof("Reaped child.\n")-1);
    }
}

int main () {
    std::printf("\n\n=== ZeroK Server ===\n\n");

    constexpr char MY_PORT[]  { "5555" };   // client will connect to this port

    // create struct addrinfo
    addrinfo hints {}, *res;

    hints.ai_family     =   AF_INET;
    hints.ai_socktype   =   SOCK_STREAM;
    hints.ai_flags      =   AI_PASSIVE;

    getaddrinfo (nullptr, MY_PORT, &hints, &res);

    // 1. create a socket
    int socketfd = socket (res->ai_family, res->ai_socktype, res->ai_protocol);

    // make sure socket is reused for subsequent connections
    std::uint8_t yes { 1 };
    setsockopt (socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));


    // 2. bind to port we passed in getaddrinfo
    bind (socketfd, res->ai_addr, res->ai_addrlen);


    // 3. listen for incoming requests from client
    // SOMAXCONN = let kernel decide apt size of waiting queue
    listen (socketfd, SOMAXCONN);

    std::printf("\nWaiting for connection...\n");


    // 4. accept request - TCP handshake
    // accept() returns new socket for client connection
    sockaddr_storage cli_addr {};
    socklen_t addr_size = sizeof (cli_addr);

    // register zombie_reaper
    struct sigaction sa {};
    sa.sa_sigaction = zombie_reaper;
    sa.sa_flags     = SA_SIGINFO | SA_RESTART;   // get child info + auto restart syscalls

    // dont block other signals during zombie reaping
    sigemptyset (&sa.sa_mask);

    if (sigaction (SIGCHLD, &sa, nullptr) == -1) {
        perror ("sigaction");
        return 1;
    }

    // multiple accepts/multiple clients
    while (true) {
        int newfd = accept (socketfd, (sockaddr*)&cli_addr, &addr_size); 

        // make client socket non blocking
        int flags = fcntl (newfd, F_GETFL, 0);
        fcntl (newfd, F_SETFL, flags | O_NONBLOCK);

        pid_t pid = fork();

        if (pid == 0) {
            // **child process
            close (socketfd);   // child doesnt need listening socket

            std::printf("\n\nClient connected.\n");
            std::printf("Process %d handling client.\n", getpid());

            // 5. send
            const char* msg { "Connected to ZeroK server.\n" };
            send (newfd, msg, strlen(msg), 0);

            // 6. recv
            char buff [1024];   // recv bytes from client in buff storage
            int r;              // r = how many bytes recv'd

            while ( (r = recv (newfd, buff, sizeof(buff), 0)) > 0 ) {
                // echo back same bytes recv'd in buff
                send (newfd, buff, r, 0);

                // optional: debug print
                write (1, buff, r);
            }

            close (newfd);
            exit(0);
        }

        // **parent process
        // 7. close
        close (newfd);      // parent doesnt need connected socket
    }

    close (socketfd);

    std::printf("\n\n");
    return EXIT_SUCCESS;
}
