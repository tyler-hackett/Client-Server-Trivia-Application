// Author: Tyler Hackett
// Pledge: I pledge my honor that I have abided by the Stevens Honor System.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <errno.h>
#include <sys/select.h>
#define BUFFER_SIZE 1024

void connect_to_server(int argc, char* argv[], int* sockfd) {
    char* ip = "127.0.0.1";
    int port = 25555;
    int opt;
    while ((opt = getopt(argc, argv, "i:p:h")) != -1) {
        switch (opt) {
            case 'i':
                ip = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'h':
                printf("Usage: %s [-i IP_address] [-p port_number] [-h]\n", argv[0]);
                printf("-i IP_address\tDefault: 127.0.0.1\n");
                printf("-p port_number\tDefault: 25555\n");
                printf("-h\t\tShow this help message\n");
                exit(0);
            case '?':
                fprintf(stderr, "Error: Unknown option '%c' received\n", optopt);
                exit(1);
        }
    }
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd < 0) {
        perror("socket failed");
        exit(1);
    }
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = inet_addr(ip)
    };
    if (connect(*sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        close(*sockfd);
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    int sockfd;
    connect_to_server(argc, argv, &sockfd);
    fd_set read_fds;
    int max_fd = sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO;
    char buffer[BUFFER_SIZE];
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select error");
            break;
        }
        if (FD_ISSET(sockfd, &read_fds)) {
            int bytes = read(sockfd, buffer, sizeof(buffer) - 1);
            if (bytes <= 0) {
                printf("\nServer disconnected\n");
                break;
            }
            buffer[bytes] = '\0';
            printf("\n%s", buffer);
            if (strstr(buffer, "name:")) {
                printf("Enter your name: ");
                fflush(stdout);
                char name[128];
                if (fgets(name, sizeof(name), stdin)) {
                    name[strcspn(name, "\n")] = '\0';
                    write(sockfd, name, strlen(name));
                }
            }
        }
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char input[10];
            if (fgets(input, sizeof(input), stdin)) {
                if (input[0] >= '1' && input[0] <= '3') {
                    write(sockfd, input, 1);
                }
            }
        }
    }
    close(sockfd);
    return 0;
}