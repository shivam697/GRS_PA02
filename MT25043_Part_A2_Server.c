// MT25043
//
// File: MT25043_Part_A2_Server.c (ROLE: SENDER)
//
// Description: One-Copy TCP Server. This server sends structured messages
// with 8 dynamically allocated fields using sendmsg() with iovec.
//
// ============================================================================
// AI USAGE DECLARATION
// ============================================================================
// Tool: GitHub Copilot
//
// Components Where AI Assisted:
// - sendmsg() system call syntax and msghdr structure setup
// - iovec array configuration for scatter-gather I/O
// - One-copy optimization pattern implementation
//
// Prompts Used:
// - "Implement sendmsg() with iovec for scatter-gather"
// - "Configure struct msghdr for multiple buffers"
//
// Student Understanding:
// - Understands scatter-gather I/O: kernel assembles from multiple buffers
// - Understands one-copy optimization: eliminates intermediate buffer copy
// - Can explain iovec structure: iov_base (pointer) and iov_len (size)
// - Can explain difference from two-copy: no memcpy() needed
// - Ready to explain sendmsg() vs send() during viva
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/uio.h> // For struct iovec

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

    // Prepare message for sending (One-Copy method with sendmsg)
    int field_size = g_msg_size / NUM_FIELDS;
    message_t* msg = create_message(field_size);
    if (!msg) {
        close(client_socket);
        return NULL;
    }

    struct iovec iov[NUM_FIELDS];
    for (int i = 0; i < NUM_FIELDS; i++) {
        iov[i].iov_base = msg->field[i];
        iov[i].iov_len = field_size;
    }

    struct msghdr msg_hdr;
    memset(&msg_hdr, 0, sizeof(msg_hdr));
    msg_hdr.msg_iov = iov;
    msg_hdr.msg_iovlen = NUM_FIELDS;

    // Send messages repeatedly for the specified duration
    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);

    while (1) {
        gettimeofday(&current_time, NULL);
        if (current_time.tv_sec - start_time.tv_sec >= g_duration) {
            break;
        }

        ssize_t bytes_sent = sendmsg(client_socket, &msg_hdr, 0);
        if (bytes_sent <= 0) {
            // Client disconnected or send failed
            break;
        }
    }

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