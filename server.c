#include "server.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

// Initialize server statistics
int request_count = 0;
int received_bytes = 0;
int sent_bytes = 0;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

void start_server(int port) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Create the server socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    // Set the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind the socket to the specified port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Binding failed");
        close(server_socket);
        exit(1);
    }

    // Start listening for incoming connections
    if (listen(server_socket, 10) == -1) {
        perror("Listening failed");
        close(server_socket);
        exit(1);
    }

    printf("Server started on port %d\n", port);

    // Accept and handle client connections in a loop
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("Accept failed");
            continue;
        }

        // Create a new thread for handling the client request
        pthread_t thread_id;
        ClientData *client_data = malloc(sizeof(ClientData));
        client_data->client_socket = client_socket;
        if (pthread_create(&thread_id, NULL, handle_client, client_data) != 0) {
            perror("Thread creation failed");
            free(client_data);
            close(client_socket);
        }

        // Detach the thread to avoid memory leaks
        pthread_detach(thread_id);
    }

    close(server_socket);
}

void update_stats(int received, int sent) {
    pthread_mutex_lock(&stats_mutex);
    request_count++;
    received_bytes += received;
    sent_bytes += sent;
    pthread_mutex_unlock(&stats_mutex);
}

void send_response(int client_socket, const char *status, const char *content_type, const char *body) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n"
             "%s", status, content_type, strlen(body), body);
    send(client_socket, buffer, strlen(buffer), 0);

    // Update statistics
    update_stats(strlen(body), strlen(buffer));
}

void send_file_response(int client_socket, const char *file_path) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        send_response(client_socket, "404 Not Found", "text/html", "<h1>404 Not Found</h1>");
        return;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Send HTTP response headers
    char file_buffer[BUFFER_SIZE];
    snprintf(file_buffer, sizeof(file_buffer),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/octet-stream\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n\r\n", file_size);
    send(client_socket, file_buffer, strlen(file_buffer), 0);

    // Send file content
    size_t bytes;
    while ((bytes = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
        send(client_socket, file_buffer, bytes, 0);
    }

    fclose(file);

    // Update statistics
    update_stats(file_size, strlen(file_buffer) + file_size);
}

void handle_request(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        close(client_socket);
        return;
    }
    buffer[bytes_read] = '\0';

    // Parse the HTTP request (method and path)
    char method[16], path[256];
    if (sscanf(buffer, "%s %s", method, path) != 2) {
        send_response(client_socket, "400 Bad Request", "text/html", "<h1>400 Bad Request</h1>");
        close(client_socket);
        return;
    }

    update_stats(bytes_read, 0);

    if (strcmp(path, "/stats") == 0) {
        char stats_body[512];
        pthread_mutex_lock(&stats_mutex);
        snprintf(stats_body, sizeof(stats_body),
                 "<html><body><h1>Server Stats</h1><p>Requests: %d</p><p>Received Bytes: %d</p><p>Sent Bytes: %d</p></body></html>",
                 request_count, received_bytes, sent_bytes);
        pthread_mutex_unlock(&stats_mutex);
        send_response(client_socket, "200 OK", "text/html", stats_body);

    } else if (strncmp(path, "/static/", 8) == 0) {
        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s%s", STATIC_DIR, path + 7); // Skip "/static/"
        send_file_response(client_socket, file_path);

    } else if (strncmp(path, "/calc?", 6) == 0) {
        int a, b;
        if (sscanf(path, "/calc?a=%d&b=%d", &a, &b) != 2) {
            send_response(client_socket, "400 Bad Request", "text/html", "<h1>400 Bad Request</h1>");
        } else {
            char calc_body[256];
            snprintf(calc_body, sizeof(calc_body), "<html><body><h1>Calculation Result</h1><p>%d + %d = %d</p></body></html>", a, b, a + b);
            send_response(client_socket, "200 OK", "text/html", calc_body);
        }

    } else {
        send_response(client_socket, "404 Not Found", "text/html", "<h1>404 Not Found</h1>");
    }

    close(client_socket);
}

void *handle_client(void *client_data) {
    ClientData *data = (ClientData *)client_data;
    handle_request(data->client_socket);
    free(data);
    return NULL;
}

