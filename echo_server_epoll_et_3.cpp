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

