#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define STATIC_DIR "./static"  // Directory for static files

typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} ClientData;

// Server statistics variables (accessed by multiple threads)
extern int request_count;
extern int received_bytes;
extern int sent_bytes;
extern pthread_mutex_t stats_mutex;

// Function declarations
void start_server(int port);
void *handle_client(void *client_data);
void handle_request(int client_socket);
void send_response(int client_socket, const char *status, const char *content_type, const char *body);
void send_file_response(int client_socket, const char *file_path);
void update_stats(int received, int sent);

#endif