// Ch 11 CSAPP Networking// 29.04.26// ZeroK
#include <sys/types.h>
#include <sys/socket.h>
#include <cstdlib>
#include <cstdint>
#include <cstddef>

int main() {

    std::printf("\n\n");
    
    // socket
    int socketfd = socket (AF_INET, SOCK_STREAM, 0);

    const char* ip = "142.250.192.206";

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port   = htons (80);


    inet_pton (AF_INET, ip, &address.sin_addr);


    // connect
    int result = connect (socketfd, (sockaddr*)&address, sizeof(address));

    if (result != 0) {
        perror("connect");
        return 1;
    }


    // send
    const char* message =  "GET / HTTP/1.1\r\n"
                           "Host: google.com\r\n"
                           "Connection: close\r\n\r\n";

    send (socketfd, message, strlen(message), 0);


    // receive
    char buffer [1024];
    int n = recv (socketfd, buffer, sizeof(buffer) - 1, 0);

    while (n > 0) {
        buffer[n] = '\0';
        write (1, buffer, n);
        std::printf("[ recv chunk %d bytes ]\n", n);
    }


    
    // close
    close (socketfd);

    std::printf("\n\n");
    return EXIT_SUCCESS;

}

