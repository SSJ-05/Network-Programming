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



    return EXIT_SUCCESS;
}

