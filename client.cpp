// Ch 11 CSAPP Networking// 29.04.26// ZeroK
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

constexpr char[] SERVERPORT    { "5555" };
constexpr char[] SERVADDR      { "127.0.0.1" };
constexpr int    BUFFER_SIZE   { 1024 }; 


int main() {

    std::printf("\n\n");
    
    // 1. create a socket with IPv4, TCP stream
    int socketfd = socket (AF_INET, SOCK_STREAM, 0);

    // target server - google.com
    // const char* ip = "142.250.192.206";
    const char[] ip { SERVADDR };

    // build addr struct
    sockaddr_in address {};
    address.sin_family = AF_INET;        // IPv4
    // address.sin_port   = htons (80);     // convert host to network byte order(big endian) on port 80
    address.sin_port   = htons (SERVERPORT);


    inet_pton (AF_INET, ip, &address.sin_addr);    // convert ip string to binary


    // 2. connect to target server
    int result = connect (socketfd, (sockaddr*)&address, sizeof(address));

    if (result != 0) {
        perror("connect");
        return 1;
    }


    // // build HTTP request
    // const char* message =  "GET / HTTP/1.1\r\n"
    //                        "Host: google.com\r\n"
    //                        "Connection: close\r\n\r\n";
    const char[] message { "Client says hello" };

    // 3. send request to target server
    send (socketfd, message, strlen(message), 0);


    // 4. receive response from server
    char buffer [BUFFER_SIZE];        // store raw bytes from TCP stream
    
    int n;
    while ( (n = recv (socketfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        // buffer[n] = '\0';            // make it valid c-string
        write (STDOUT_FILENO, buffer, n);        // print raw bytes
        // std::printf("[ recv chunk %d bytes ]\n", n);
    }


    
    // 5. close the socket/connection
    close (socketfd);

    std::printf("\n\n");
    return EXIT_SUCCESS;

}

