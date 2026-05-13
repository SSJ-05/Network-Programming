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

// TCP is not packet transmission - its byte river
// we need to define message boundaries, parsing, framing


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cerrno>



constexpr char MYPORT[]     { "5555" };
constexpr int  MAX_CLI      { 32 };         // max simultaneous clients allowed
constexpr int  MAX_EVENTS   { 64 };         // max events for polling allowed
constexpr int  MAX_FD       { 65536 };                                            
static    int  client_count { 0 };



// client state - ring buffer
struct Client {
    alignas (64) char out_buf [8192];
    
    std::uint32_t   write_pos  {};
    std::uint32_t   send_pos   {};

    bool active {};
};

// each fd has now persistent state
Client clients [MAX_FD];
bool active [MAX_FD];



// TCP is a stream, kernel might withold data
// send() doesnt send all the data
// handle partial send from beej's guide for blocking sockets
// skt = socket fd, buf = storage buffer, len = no. of bytes in buf
int send_all (int skt, char* buf, int* len) {
    int total = 0;          // total bytes sent
    int bytesleft = *len;   // bytes left to be sent
    int n;

    // keep send()ing until original payload is empty
    while (total < *len) {
        n = send (skt, buf + total, bytesleft, 0);  // buf + total = begin sending from unsent portion
        if (n == -1) break;
        total += n;         // update total bytes sent
        bytesleft -= n;     // update bytes left
    }

    *len = total;           // actual number of bytes sent
    return n == -1 ? EXIT_FAILURE : EXIT_SUCCESS; 
}


// helper func for flushing buffer
void flush_send_buffer (int fd, epoll_event& ev, int epfd, Client& client) 
{
    while (client.send_pos < client.write_pos) 
    {
        int sent = send (fd, 
                        client.out_buf   + client.send_pos,
                        client.write_pos - client.send_pos,
                        0);

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
            clients[fd].active = false;
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



int main() {

    std::printf("\n=== Edge Triggered epoll server ===\n");
    
    // getaddrinfo
    addrinfo hints {}, *res;    // hints = socket properties, res = linked list returned by kernel
    
    hints.ai_family     =   AF_
