#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 12345

int client_socket;
volatile int running = 1;  

void *receive_messages(void *socket_desc) {
    int sock = *(int *)socket_desc;
    char buffer[1024];
    int read_size;

    while (running) {
        read_size = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (read_size > 0) {
            buffer[read_size] = '\0';
            printf("\n[New Message]: %s\n", buffer);
            printf("Enter recipient_id:message: ");  
            fflush(stdout);
        } else if (read_size == 0) {
            printf("\nServer disconnected.\n");
            running = 0;
            break;
        }
    }
    return NULL;
}

int main() {
    struct sockaddr_in server_addr;
    char buffer[1024];
    pthread_t receive_thread;


    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);


    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connect failed");
        return 1;
    }


    pthread_create(&receive_thread, NULL, receive_messages, (void *)&client_socket);

    while (running) {
        printf("Enter recipient_id:message (or 'exit' to quit): ");
        fgets(buffer, sizeof(buffer), stdin);

        if (strcmp(buffer, "exit\n") == 0) {
            running = 0;
            send(client_socket, buffer, strlen(buffer), 0);
            break;
        }

        send(client_socket, buffer, strlen(buffer), 0);
    }


    close(client_socket);
    pthread_join(receive_thread, NULL);
    return 0;
}

