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

ClientInfo clients[MAX_CLIENTS] = {0};  
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void send_client_list(int client_socket) {
    char client_list[1024] = "Connected clients:\n";
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != 0) {
            char id[20];
            sprintf(id, "Client ID: %d\n", i);
            strcat(client_list, id);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    send(client_socket, client_list, strlen(client_list), 0);
}

int insert_client(int client_socket) {
    int client_index = -1;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == 0) {
            clients[i].socket = client_socket;
            clients[i].recipient_id = -1;  
            client_index = i;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return client_index;
}

void remove_client(int client_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == client_socket) {
            clients[i].socket = 0;
            clients[i].recipient_id = -1;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

int find_client_index(int client_socket) {
    int index = -1;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == client_socket) {
            index = i;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return index;
}

void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[1024];
    int read_size;
    int client_index;
    free(arg);

    client_index = insert_client(client_socket);
    if (client_index == -1) {
        send(client_socket, "Server full. Try again later.\n", 30, 0);
        close(client_socket);
        return NULL;
    }

    char welcome[100];
    sprintf(welcome, "Welcome to the server! Your client ID is: %d\n", client_index);
    send(client_socket, welcome, strlen(welcome), 0);
    send(client_socket, "Type 'exit' to quit\n", 20, 0);
    send_client_list(client_socket);

    
    while (1) {
        send(client_socket, "Enter /target <id> to select chat recipient: ", 45, 0);
        
        read_size = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (read_size <= 0) {
            break; 
        }
        
        buffer[read_size] = '\0';
        
        if (strncmp(buffer, "/target", 7) == 0) {
            int target_id;
            if (sscanf(buffer, "/target %d", &target_id) == 1) {
                
                pthread_mutex_lock(&clients_mutex);
                int valid_target = 0;
                if (target_id >= 0 && target_id < MAX_CLIENTS && clients[target_id].socket != 0) {
                    clients[client_index].recipient_id = target_id;
                    valid_target = 1;
                }
                pthread_mutex_unlock(&clients_mutex);
                
                if (valid_target) {
                    char confirm[100];
                    sprintf(confirm, "Now chatting with client ID: %d\n", target_id);
                    send(client_socket, confirm, strlen(confirm), 0);
                    break; 
                } else {
                    send(client_socket, "Invalid target ID. Try again.\n", 30, 0);
                }
            } else {
                send(client_socket, "Usage: /target <id>\n", 20, 0);
            }
        } else if (strcmp(buffer, "exit\n") == 0) {
            remove_client(client_socket);
            close(client_socket);
            return NULL;
        } else {
            send(client_socket, "Please select a target first using /target <id>\n", 47, 0);
        }
    }

    
    while (1) {
        read_size = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (read_size <= 0) {
            break; 
        }
        
        buffer[read_size] = '\0';
        
        if (strcmp(buffer, "exit\n") == 0) {
            break;
        }
        
        if (strcmp(buffer, "/change\n") == 0) {
            
            pthread_mutex_lock(&clients_mutex);
            clients[client_index].recipient_id = -1;
            pthread_mutex_unlock(&clients_mutex);
            
            send(client_socket, "Changing recipient...\n", 22, 0);
            send_client_list(client_socket);
            
            
            while (1) {
                send(client_socket, "Enter /target <id> to select chat recipient: ", 45, 0);
                
                read_size = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
                if (read_size <= 0) {
                    break;
                }
                
                buffer[read_size] = '\0';
                
                if (strncmp(buffer, "/target", 7) == 0) {
                    int target_id;
                    if (sscanf(buffer, "/target %d", &target_id) == 1) {
                        pthread_mutex_lock(&clients_mutex);
                        int valid_target = 0;
                        if (target_id >= 0 && target_id < MAX_CLIENTS && clients[target_id].socket != 0) {
                            clients[client_index].recipient_id = target_id;
                            valid_target = 1;
                        }
                        pthread_mutex_unlock(&clients_mutex);
                        
                        if (valid_target) {
                            char confirm[100];
                            sprintf(confirm, "Now chatting with client ID: %d\n", target_id);
                            send(client_socket, confirm, strlen(confirm), 0);
                            break;
                        } else {
                            send(client_socket, "Invalid target ID. Try again.\n", 30, 0);
                        }
                    }
                } else if (strcmp(buffer, "exit\n") == 0) {
                    remove_client(client_socket);
                    close(client_socket);
                    return NULL;
                }
            }
            
            if (read_size <= 0) {
                break;
            }
            
            continue;
        }
        
        
        pthread_mutex_lock(&clients_mutex);
        int recipient_id = clients[client_index].recipient_id;
        int valid_recipient = (recipient_id >= 0 && recipient_id < MAX_CLIENTS && 
                              clients[recipient_id].socket != 0);
        pthread_mutex_unlock(&clients_mutex);
        
        if (!valid_recipient) {
            send(client_socket, "No valid recipient. Use /change to select a new one.\n", 52, 0);
            continue;
        }
        
        
        char formatted_msg[1100];
        sprintf(formatted_msg, "Message from Client %d: %s", client_index, buffer);
        
        pthread_mutex_lock(&clients_mutex);
        if (clients[recipient_id].socket != 0) {
            send(clients[recipient_id].socket, formatted_msg, strlen(formatted_msg), 0);
            send(client_socket, "Message sent.\n", 14, 0);
        } else {
            send(client_socket, "Recipient disconnected. Use /change to select a new one.\n", 57, 0);
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    printf("Client %d disconnected\n", client_index);
    remove_client(client_socket);
    close(client_socket);
    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t client_thread;
    
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = 0;
        clients[i].recipient_id = -1;
    }

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        return 1;
    }
    
    
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
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

    while ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len)) > 0) {
        printf("New client connected: %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        int *client_socket_ptr = malloc(sizeof(int));
        if (client_socket_ptr == NULL) {
            perror("Malloc failed");
            close(client_socket);
            continue;
        }
        *client_socket_ptr = client_socket;

        if (pthread_create(&client_thread, NULL, client_handler, (void *)client_socket_ptr) < 0) {
            perror("Could not create thread");
            free(client_socket_ptr);
            close(client_socket);
            continue;
        }

        pthread_detach(client_thread);
    }

    if (client_socket < 0) {
        perror("Accept failed");
        return 1;
    }

    close(server_socket);
    return 0;
}
