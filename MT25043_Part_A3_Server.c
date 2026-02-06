// MT25043
//
// File: MT25043_Part_A3_Server.c (ROLE: SENDER)
//
// Description: Zero-Copy TCP Server. This server sends structured messages
// with 8 dynamically allocated fields using sendmsg() with MSG_ZEROCOPY.
//
// ============================================================================
// AI USAGE DECLARATION
// ============================================================================
// Tool: GitHub Copilot
//
// Components Where AI Assisted:
// - MSG_ZEROCOPY flag usage and SO_ZEROCOPY socket option
// - Error queue handling with MSG_ERRQUEUE for completion notifications
// - recvmsg() for draining completion queue
//
// Prompts Used:
// - "Implement MSG_ZEROCOPY with completion handling"
// - "Drain error queue with MSG_ERRQUEUE and MSG_DONTWAIT"
//
// Student Understanding:
// - Understands zero-copy: kernel references user buffers via DMA
// - Understands async completion: buffer freed only after NIC DMA completes
// - Can explain MSG_ERRQUEUE: notification mechanism for zero-copy operations
// - Can explain why error queue draining prevents buffer overflow
// - Understands kernel requirement: Linux 4.14+ for MSG_ZEROCOPY support
// - Ready to explain zero-copy vs one-copy tradeoffs during viva
// ============================================================================

#define _GNU_SOURCE // Required for MSG_ZEROCOPY and SO_ZEROCOPY

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/uio.h> // For struct iovec
#include <sys/socket.h> // For sendmsg
#include <errno.h>
#include <linux/errqueue.h> // For SO_EE_ORIGIN_ZEROCOPY

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

    // Prepare message for sending (Zero-Copy method with MSG_ZEROCOPY)
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

        ssize_t bytes_sent = sendmsg(client_socket, &msg_hdr, MSG_ZEROCOPY);
        if (bytes_sent <= 0) {
            // Client disconnected or send failed
            break;
        }

        // Optionally drain error queue for zero-copy completions
        // (simplified - not strictly necessary for this assignment)
        char cmsg_buf[CMSG_SPACE(sizeof(struct sock_extended_err))];
        struct msghdr r_msg_hdr;
        struct iovec r_iov;
        char dummy_buffer[1];
        
        r_iov.iov_base = dummy_buffer;
        r_iov.iov_len = sizeof(dummy_buffer);
        memset(&r_msg_hdr, 0, sizeof(r_msg_hdr));
        r_msg_hdr.msg_iov = &r_iov;
        r_msg_hdr.msg_iovlen = 1;
        r_msg_hdr.msg_control = cmsg_buf;
        r_msg_hdr.msg_controllen = sizeof(cmsg_buf);
        
        recvmsg(client_socket, &r_msg_hdr, MSG_ERRQUEUE | MSG_DONTWAIT);
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

    // Enable SO_ZEROCOPY on server socket
    int zero_copy_opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_ZEROCOPY, &zero_copy_opt, sizeof(zero_copy_opt)) < 0) {
        perror("setsockopt(SO_ZEROCOPY) failed - continuing without zero-copy");
        // Continue anyway - zero-copy may not be supported
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