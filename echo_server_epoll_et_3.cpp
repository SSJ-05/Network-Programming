#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cerrno>



constexpr char MYPORT[]         { "5555" };
constexpr int  MAX_CLI          { 32 };         // max simultaneous clients allowed
constexpr int  MAX_EVENTS       { 64 };         // max events for polling allowed
constexpr int  MAX_FD           { 65536 };                                            
static    int  client_count     { 0 };



// client state - ring buffer
struct Client 
{
    alignas(64) char out_buf [8192];
    
    std::uint32_t   write_pos  {};      // head
    std::uint32_t   send_pos   {};      // tail

    char* get_write_ptr () { return out_buf + (write_pos & 8191); }
};

// each fd has now persistent state
Client  clients [MAX_FD];



// helper func for flushing buffer
void flush_send_buffer (int fd, epoll_event& ev, int epfd, Client& client) 
{
    while (client.send_pos < client.write_pos) 
    {
        char* ptr   =   client.out_buf + (client.send_pos & 8191);
        int avl     =   8192 - (client.send_pos & 8191);       
        int sent    =   send (fd, ptr, avl, MSG_DONTWAIT);

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
    client.send_pos  = 0;
    client.write_pos = 0;

    ev.events  = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.data.fd = fd;
    epoll_ctl (epfd, EPOLL_CTL_MOD, fd, &ev);
}


int main() 
{
    std::printf("\n=== Edge Triggered epoll server ===\n");
    
    // getaddrinfo
    addrinfo hints {}, *res;    // hints = socket properties, res = linked list returned by kernel
    
    hints.ai_family     =   AF_INET;
    hints.ai_socktype   =   SOCK_STREAM;
    hints.ai_flags      =   AI_PASSIVE;

    // getaddrinfo (nullptr, MYPORT, &hints, &res);


    
    // socket
    // int listener = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
    int listener = socket (AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    // make listener socket non blocking
    // int flag_listener = fcntl (listener, F_GETFL, 0);
    // fcntl (listener, F_SETFL, flag_listener | O_NONBLOCK);

    // quick rebinding to the same port after restart
    int yes { 1 };
    setsockopt (listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    bind (listener, res->ai_addr, res->ai_addrlen);

    listen (listener, SOMAXCONN);

    freeaddrinfo (res);



    // create epoll instance/ kernel event manager
    int epfd { epoll_create1(0) };
    
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

    if (epoll_ctl (epfd, EPOLL_CTL_ADD, listener, &ev) == -1) {
        perror ("epoll_ctl");
        exit(EXIT_FAILURE);
    }

    std::printf("\nWaiting for connection...\n");




    
    std::printf("\n\n");
    return EXIT_SUCCESS;
}
