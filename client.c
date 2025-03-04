#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 12345
#define SERVER_IP "127.0.0.1"

int client_socket;
volatile int running = 1;
pthread_mutex_t screen_mutex = PTHREAD_MUTEX_INITIALIZER;

void *receive_messages(void *socket_desc) {
    int sock = *(int *)socket_desc;
    char buffer[1024];
    int read_size;

    while (running) {
        read_size = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (read_size > 0) {
            buffer[read_size] = '\0';
            
            pthread_mutex_lock(&screen_mutex);
            printf("\n%s", buffer);
            printf("You: ");
            fflush(stdout);
            pthread_mutex_unlock(&screen_mutex);
            
        } else if (read_size == 0) {
            pthread_mutex_lock(&screen_mutex);
            printf("\nServer disconnected.\n");
            pthread_mutex_unlock(&screen_mutex);
            
            running = 0;
            break;
        } else {
            pthread_mutex_lock(&screen_mutex);
            printf("\nError receiving message.\n");
            pthread_mutex_unlock(&screen_mutex);
            
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
        perror("Socket creation failed");
        return 1;
    }

    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    
    printf("Connecting to server at %s:%d...\n", SERVER_IP, PORT);
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connect failed");
        return 1;
    }
    printf("Connected to server.\n");

    
    if (pthread_create(&receive_thread, NULL, receive_messages, (void *)&client_socket) < 0) {
        perror("Thread creation failed");
        close(client_socket);
        return 1;
    }

    
    while (running) {
        pthread_mutex_lock(&screen_mutex);
        printf("You: ");
        pthread_mutex_unlock(&screen_mutex);
        
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }

        
        if (strcmp(buffer, "exit\n") == 0) {
            running = 0;
            send(client_socket, buffer, strlen(buffer), 0);
            break;
        }

        
        if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
            perror("Send failed");
            running = 0;
            break;
        }
    }

    printf("Disconnecting from server...\n");
    close(client_socket);
    pthread_join(receive_thread, NULL);
    printf("Disconnected. Goodbye!\n");
    
    return 0;
}
