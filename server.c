#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 12345
#define MAX_CLIENTS 10

typedef struct {
    int socket;
    int recipient_id;
} ClientInfo;

ClientInfo clients[MAX_CLIENTS] = {0};  // Stores socket and target recipient

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int running = 1;

void send_client_list(int client_socket) {
    char client_list[1024] = "Connected clients:\n";
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != 0) {
            char id[20];
            sprintf(id, "%d\n", clients[i].socket);
            strcat(client_list, id);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    send(client_socket, client_list, strlen(client_list), 0);
}

void insert_client(int client_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == 0) {
            clients[i].socket = client_socket;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int client_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket== client_socket) {
            clients[i].socket = 0;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[1024];
    int read_size;
    free(arg);  

    insert_client(client_socket);

    send(client_socket, "Welcome to the server!\n", 23, 0);
    send(client_socket, "Type 'exit' to quit\n", 20, 0);
    send(client_socket, "Connected clients:\n", 19, 0);
    send_client_list(client_socket);

    while (1) {
        send(client_socket, "Enter /target <id> to chat(or 'exit' to quit or /change to change recipient): ", 70, 0);


        read_size = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (read_size <= 0) {
            break;  
        }

        buffer[read_size] = '\0';

        if (strcmp(buffer, "exit\n") == 0) {
            break;
        }

        int target_id;
        char message[1000];

        if (sscanf(buffer, "/target %d", &target_id) == 1) {
            printf("Target set to %d\n", target_id);
            clients[client_socket].recipient_id = target_id;
            continue;  // Don't treat it as a message
        }

        running = 1;

        if (sscanf(buffer, "%*s %[^\n]", message) && clients[client_socket].recipient_id ) {
            send(client_socket, "Invalid message.\n", 17, 0);
            continue;
        }

        if (strcmp(message, "/change\n") == 0) {
            running = 0;
            break;
        }

        int found = 0;
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket == client_socket) {
                if (clients[i].recipient_id == -1) {
                    send(client_socket, "No target set. Use /target <id> first.\n", 38, 0);
                    found = 1;
                } else {
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].socket == clients[i].recipient_id) {
                            send(clients[j].socket, message, strlen(message), 0);
                            found = 1;
                            break;
                        }
                    }
                }
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (!found) {
            send(client_socket, "Client not found.\n", 18, 0);
        }
    }

    remove_client(client_socket);
    close(client_socket);
    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t client_thread;
    int *client_socket_ptr;

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        return 1;
    }

    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("Listen failed");
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    while ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len))) {
        printf("Client connected: %d\n", client_socket);

        client_socket_ptr = malloc(sizeof(int));
        if (client_socket_ptr == NULL) {
            perror("Malloc failed");
            continue;
        }
        *client_socket_ptr = client_socket;

        if (pthread_create(&client_thread, NULL, client_handler, (void *)client_socket_ptr) < 0) {
            perror("Could not create thread");
            free(client_socket_ptr);
            continue;
        }

        pthread_detach(client_thread);
    }

    close(server_socket);
    return 0;
}

