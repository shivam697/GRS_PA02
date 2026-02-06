// MT25043
//
// File: MT25043_Part_A1_Server.c (ROLE: SENDER)
//
// Description: Two-Copy (Baseline) TCP Server. This server sends
// structured messages with 8 dynamically allocated fields to clients.
//
// ============================================================================
// AI USAGE DECLARATION
// ============================================================================
// Tool: GitHub Copilot
// 
// Components Where AI Assisted:
// - Socket API boilerplate (socket, bind, listen, accept setup)
// - Error handling patterns (perror, exit calls)
// - Pthread threading structure (pthread_create, detach)
// - Memory allocation patterns for message structures
//
// Prompts Used:
// - "Create TCP server socket with pthread threading"
// - "Implement send() with proper error checks"
// - "Handle client connections in separate threads"
//
// Student Understanding:
// - Understands socket lifecycle: socket() -> bind() -> listen() -> accept()
// - Understands two-copy overhead: user buffer -> intermediate buffer -> kernel
// - Can explain memory allocation for 8 fields and buffer management
// - Can explain handshake protocol and timing mechanisms
// - Ready to explain every line during viva examination
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define PORT 8080
#define NUM_FIELDS 8

// Global parameters set from command line
static int g_msg_size = 8192;
static int g_duration = 10;

// The message structure with 8 dynamically allocated string fields.
typedef struct {
    char* field[NUM_FIELDS];
} message_t;

message_t* create_message(int field_size) {
    message_t* msg = (message_t*)malloc(sizeof(message_t));
    if (!msg) {
        perror("Failed to allocate message struct");
        return NULL;
    }
    for (int i = 0; i < NUM_FIELDS; i++) {
        msg->field[i] = (char*)malloc(field_size);
        if (!msg->field[i]) {
            perror("Failed to allocate message field");
            for (int j = 0; j < i; j++) free(msg->field[j]);
            free(msg);
            return NULL;
        }
        memset(msg->field[i], 'A' + i, field_size);
    }
    return msg;
}

void free_message(message_t* msg) {
    if (msg) {
        for (int i = 0; i < NUM_FIELDS; i++) {
            if (msg->field[i]) free(msg->field[i]);
        }
        free(msg);
    }
}

void* handle_client(void* args) {
    int client_socket = *(int*)args;
    free(args);

    // *** HANDSHAKE: Wait for client "Ready" signal ***
    char ready_signal;
    if (recv(client_socket, &ready_signal, 1, 0) <= 0) {
        perror("Server: Handshake recv failed");
        close(client_socket);
        return NULL;
    }

    // *** HANDSHAKE: Send "Go" signal to client ***
    char go_signal = 'G';
    if (send(client_socket, &go_signal, 1, 0) <= 0) {
        perror("Server: Handshake send failed");
        close(client_socket);
        return NULL;
    }

    // Prepare message for sending (Two-Copy method)
    int field_size = g_msg_size / NUM_FIELDS;
    message_t* msg = create_message(field_size);
    if (!msg) {
        close(client_socket);
        return NULL;
    }

    // Copy all fields into a single send buffer
    char* send_buffer = (char*)malloc(g_msg_size);
    if (!send_buffer) {
        free_message(msg);
        close(client_socket);
        return NULL;
    }
    
    char* current_pos = send_buffer;
    for (int i = 0; i < NUM_FIELDS; i++) {
        memcpy(current_pos, msg->field[i], field_size);
        current_pos += field_size;
    }

    // Send messages repeatedly for the specified duration
    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);

    while (1) {
        gettimeofday(&current_time, NULL);
        if (current_time.tv_sec - start_time.tv_sec >= g_duration) {
            break;
        }

        ssize_t bytes_sent = send(client_socket, send_buffer, g_msg_size, 0);
        if (bytes_sent <= 0) {
            // Client disconnected or send failed
            break;
        }
    }

    free(send_buffer);
    free_message(msg);
    printf("Server: Client disconnected. Closing socket %d.\n", client_socket);
    close(client_socket);
    return NULL;
}

int main(int argc, char const *argv[]) {
    // Parse command-line arguments
    if (argc >= 2) {
        g_msg_size = atoi(argv[1]);
    }
    if (argc >= 3) {
        g_duration = atoi(argv[2]);
    }

    if (g_msg_size % NUM_FIELDS != 0) {
        fprintf(stderr, "Message size (%d) must be divisible by NUM_FIELDS (%d)\n", g_msg_size, NUM_FIELDS);
        exit(EXIT_FAILURE);
    }

    printf("Server configured: msg_size=%d bytes, duration=%d seconds\n", g_msg_size, g_duration);

    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server (Receiver) listening on port %d...\n", PORT);

    while (1) {
        int* client_socket = malloc(sizeof(int));
        if ((*client_socket = accept(server_fd, NULL, NULL)) < 0) {
            perror("accept");
            free(client_socket);
            continue;
        }

        printf("Server: New connection accepted. Socket fd is %d\n", *client_socket);

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void*)client_socket) != 0) {
            perror("pthread_create failed");
            close(*client_socket);
            free(client_socket);
        }
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}
