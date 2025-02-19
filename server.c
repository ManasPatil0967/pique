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

int clients[MAX_CLIENTS] = {0};
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void insert_client(int client_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == 0) {
            clients[i] = client_socket;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int client_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == client_socket) {
            clients[i] = 0;
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

    while (1) {
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] != 0) {
                char client_id[20];
                sprintf(client_id, "%d\n", clients[i]);
                send(client_socket, client_id, strlen(client_id), 0);
            }
        }
        pthread_mutex_unlock(&clients_mutex);

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

        if (sscanf(buffer, "%d:%[^\n]", &target_id, message) != 2) {
            send(client_socket, "Invalid format. Use: <client_id>:<message>\n", 42, 0);
            continue;
        }

        int found = 0;
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] == target_id) {
                send(clients[i], message, strlen(message), 0);
                found = 1;
                break;
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

