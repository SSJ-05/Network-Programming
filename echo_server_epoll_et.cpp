// epoll echo server// 10.05.26// ZeroK

// hows its diff from select/poll server?
// select/poll scans all sockets - even inactive ones in O(n) - wasteful
// epoll - kernel maintains a ready list of active sockets
// 3 syscalls - epoll_create1 = create a kernel event manager
//              epoll_ctl = register/remove sockets
//              epoll_wait = ask kernel for ready sockets

// level triggered = unread data remains active, kernel keeps notifying, repeated wakeups
// edge triggered  = kernel notifies once, requires non blocking sockets and 'read until EAGAIN' loop
// "WE NEED TO DRAIN THE SOCKETS FULLY"
// et reduces syscall overhead, repeated notification spam, fewer kernel wakeups

// NEW FIXES: partial send(), backpressure, write readiness, connection state machines
// UPDATE 29.05.26: introduced graceful shutdown using signals and epoll_pwait()

// TCP is not packet transmission - its byte river
// we need to define message boundaries, parsing, framing


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cerrno>

#include <algorithm>



constexpr char MYPORT[]       { "5555" };
constexpr int  MAX_CLI        { 32 };         // max simultaneous clients allowed
constexpr int  MAX_EVENTS     { 64 };         // max events for polling allowed
constexpr int  MAX_FD         { 65536 };                                            
constexpr int  BUF_SIZE       { 8192 };
constexpr int  BUF_MASK       { BUF_SIZE - 1 };
static    int  client_count   { 0 };



// client state - ring buffer
struct Client 
{
    alignas(64)
    char            out_buf    [BUF_SIZE];

    std::uint32_t   write_pos  { 0 };      // head
    std::uint32_t   send_pos   { 0 };      // tail
    bool            active     { false };  // track active status 

    char* get_write_ptr () { 
        return out_buf + (write_pos & BUF_MASK); 
    }
};

// each fd has now persistent state
Client clients [MAX_FD];



// TCP is a stream, kernel might withold data
// send() doesnt send all the data
// handle partial send from beej's guide for blocking sockets
// skt = socket fd, buf = storage buffer, len = no. of bytes in buf
// int send_all (int skt, char* buf, int* len) {
//     int total = 0;          // total bytes sent
//     int bytesleft = *len;   // bytes left to be sent
//     int n;
//
//     // keep send()ing until original payload is empty
//     while (total < *len) {
//         n = send (skt, buf + total, bytesleft, MSG_DONTWAIT);  // buf + total = begin sending from unsent portion
//         if (n == -1) break;
//         total += n;         // update total bytes sent
//         bytesleft -= n;     // update bytes left
//     }
//
//     *len = total;           // actual number of bytes sent
//     return n == -1 ? EXIT_FAILURE : EXIT_SUCCESS; 
// }


// helper func for flushing buffer
void flush_send_buffer (int fd, epoll_event& ev, int epfd, Client& client) 
{
    while (client.send_pos < client.write_pos) 
    {
        char* ptr   =   client.out_buf + (client.send_pos & BUF_MASK);
        int avl     =   BUF_SIZE - (client.send_pos & BUF_MASK);       
        int sent    =   send (fd, ptr, avl, MSG_DONTWAIT | MSG_NOSIGNAL);

        // bytes sent successfully
        if (sent > 0) client.send_pos += sent;

        // kernel send buffer full
        else if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {             // enable EPOLLOUT notification
            ev.events  = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
            ev.data.fd = fd;
            epoll_ctl (epfd, EPOLL_CTL_MOD, fd, &ev);
            return;
        }    

        // fatal error
        else 
        {
            perror ("send");
            close (fd);
            epoll_ctl (epfd, EPOLL_CTL_DEL, fd, nullptr);
            return;
        }
    }

    // buffer fully flushed - stop EPOLLOUT notifications
    ev.events  = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.data.fd = fd;
    epoll_ctl (epfd, EPOLL_CTL_MOD, fd, &ev);
}


// ctrl + c kills server brutally - sets TCP control bits to RST = reset connection
// need graceful shutdown using sig_handler and epoll_pwait()
// sets TCP control bits to FIN = finished sending data
volatile sig_atomic_t running { 1 };

void sig_handler (int sig) {
    running = 0;
}

int main() 
{
    std::printf("\n=== Edge Triggered epoll server ===\n");

    // signal handling for graceful shutdown
    signal (SIGINT, sig_handler);
    signal (SIGTERM, sig_handler);

    sigset_t emptyset;
    sigemptyset (&emptyset);
    

    //getaddrinfo
    addrinfo hints {}, *res;    // hints = socket properties, res = linked list returned by kernel

    hints.ai_family     =   AF_INET;
    hints.ai_socktype   =   SOCK_STREAM;
    hints.ai_flags      =   AI_PASSIVE;

    getaddrinfo (nullptr, MYPORT, &hints, &res);
    


    
    // socket
    int listener = socket (res->ai_family, res->ai_socktype, res->ai_protocol);

    // make listener socket non blocking
    int flag_listener = fcntl (listener, F_GETFL, 0);
    fcntl (listener, F_SETFL, flag_listener | O_NONBLOCK);

    // quick rebinding to the same port after restart
    int yes = 1;
    setsockopt (listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    bind (listener, res->ai_addr, res->ai_addrlen);

    listen (listener, SOMAXCONN);

    freeaddrinfo (res);




    // create epoll instance/ kernel event manager
    int epfd = epoll_create1(0);
    
    if (epfd == -1) {
        perror ("epoll_create1");
        exit(EXIT_FAILURE);
    }



    // register listener socket
    epoll_event ev {};                    // ev = which events to monitor
    epoll_event events [MAX_EVENTS];      // buffer for listener + new connections

    ev.events   =   EPOLLET |             // edge triggered
                    EPOLLIN |             // notify when ready
                    EPOLLRDHUP;           // peer disconnected full/half
    ev.data.fd  =   listener;             // store fd of the event

    // add event to interest list of epfd
    if (epoll_ctl (epfd, EPOLL_CTL_ADD, listener, &ev) == -1) {
        perror ("epoll_ctl");
        exit(EXIT_FAILURE);
    }

    std::printf("\nWaiting for connection...\n");



    // event loop
    while (running) 
    {
        // int nfds = epoll_wait (epfd, events, MAX_EVENTS, -1);   // timeout arg == -1 : block until 1 fd is ready
                                                                // timeout arg == 0  : nonblocking check
                                                                // timeout arg > 0   : block for set time (eg 10 ms)

        int nfds = epoll_pwait (epfd, events, MAX_EVENTS, -1, &emptyset);

        if (nfds == -1) {
            if (errno == EINTR) continue;
            perror ("epoll_pwait");
            break;
        }

        // process only active sockets, not entire fd space
        for (int i {0}; i < nfds; ++i) 
        {
            int currentfd = events[i].data.fd;      // store current fd in event data array

            // connect new client
            if (currentfd == listener) 
            {
                while (true)
                {
                    sockaddr_storage  cli_addr  {};
                    socklen_t         addrlen   { sizeof(cli_addr) };

                    // use accept4 (linux only) for non block flag
                    int newfd = accept4 (listener, (sockaddr*)&cli_addr, &addrlen, SOCK_NONBLOCK);
                    // int newfd = accept (listener, (sockaddr*)&cli_addr, &addrlen);
                    // int flags = fcntl (newfd, F_GETFL, 0);
                    // fcntl (newfd, F_SETFL, flags | O_NONBLOCK);
 
                    if (newfd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept");
                        break;
                    }                   


                    // use TCP_NODELAY to send small packets immediately - disable Nagle's algo
                    if (client_count < MAX_CLI) {
                        int flg = 1;
                        setsockopt (newfd, IPPROTO_TCP, TCP_NODELAY, &flg, sizeof(flg));
                    }


                    if (newfd >= MAX_FD) {
                        close (newfd);
                        continue;
                    }

                    
                    if (client_count >= MAX_CLI) {
                        std::printf("\nToo many clients. Rejecting connection.\n");
                        close (newfd);
                        continue;
                    }
                    ++client_count;
                    clients[newfd].active = true;       // set the active flag true
 

                    std::printf("\nNew connection fd: %d\n", newfd);


                    epoll_event cli_ev {};
                    cli_ev.events   =   EPOLLET | EPOLLIN | EPOLLRDHUP;
                    cli_ev.data.fd  =   newfd;

                    epoll_ctl (epfd, EPOLL_CTL_ADD, newfd, &cli_ev);    // add newfd to kernel epoll ready list
                }
            }

            // recv existing client data
            // use while(true) to drain socket fully until EAGAIN, otherwise data remains stranded
            else 
            {
                // handle EPOLLOUT events
                if (events[i].events & EPOLLOUT)  
                {
                    epoll_event fev {};
                    flush_send_buffer (currentfd, fev, epfd, clients[currentfd]);
                    continue;       // skip the recv loop
                    // close (currentfd);
                    // epoll_ctl (epfd, EPOLL_CTL_DEL, currentfd, &ev);
                }

                while (true) 
                {
                    char buffer [1024];
                    // int bytes = recv (currentfd, buffer, sizeof(buffer), 0);
                    int bytes = recv (currentfd, buffer, sizeof(buffer), MSG_DONTWAIT | MSG_NOSIGNAL);

                    // actual data recv'd
                    if (bytes > 0)
                    {
                        // std::memcpy (clients[currentfd].out_buf + clients[currentfd].write_pos, buffer, bytes);
                        char* wp  = clients[currentfd].get_write_ptr();
                        int space = BUF_SIZE - (clients[currentfd].write_pos & BUF_MASK);
                        int first = std::min (bytes, space);

                        std::memcpy (wp, buffer, first);
                        if (first < bytes) std::memcpy (clients[currentfd].out_buf, buffer + first, bytes - first);
                        clients[currentfd].write_pos += bytes;

                        epoll_event ev {};
                        flush_send_buffer (currentfd, ev, epfd, clients[currentfd]);
                        write (STDOUT_FILENO, buffer, bytes);
                    }

                    // client disconnected
                    else if (bytes == 0) 
                    {
                        std::printf("\nSocket %d disconnected.\n", currentfd);
                        close (currentfd);
                        epoll_ctl (epfd, EPOLL_CTL_DEL, currentfd, nullptr);
                        
                        --client_count;
                        clients[currentfd].active = false;
                        
                        break;
                    }

                    // recv error 
                    else 
                    {  
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;     // socket fully drained
                        perror ("recv");
                        close (currentfd);
                        epoll_ctl (epfd, EPOLL_CTL_DEL, currentfd, nullptr);
                        
                        --client_count;
                        clients[currentfd].active = false;
                        
                        break;
                    }
                }
            }
        }
    }

    // graceful shutdown initiated
    // 1. stop accepting new clients
    shutdown (listener, SHUT_RDWR);
    close (listener);

    // 2. close all client sockets properly
    for (int fd {0}; fd < MAX_FD; ++fd) {
        if (clients[fd].active) {
            // set FIN
            shutdown (fd, SHUT_RDWR);
            close (fd);
            clients[fd].active = false;
            
            std::printf("\nClosing Client fd : %d\n", fd);
        }
    }

    // 3. cleanup epoll
    close (epfd);


    std::printf ("\n\n=== Graceful Shutdown ===\n\n");

    std::printf("\n\n");
    return EXIT_SUCCESS;
}

