// MT25043
//
// File: MT25043_Part_A2_Client.c (ROLE: RECEIVER)
//
// Description: One-Copy TCP Client. This client receives data from
// the server and measures throughput and latency.
//
// ============================================================================
// AI USAGE DECLARATION
// ============================================================================
// Tool: GitHub Copilot
//
// Components Where AI Assisted:
// - Performance measurement patterns (consistent with A1)
// - Receiver buffer allocation and management
//
// Prompts Used:
// - "Implement receiver with performance tracking"
//
// Student Understanding:
// - Understands receiver role: no difference from A1 (optimization is sender-side)
// - Can explain why client code is similar across implementations
// - Understands one-copy benefit manifests in server's sendmsg()
// - Ready to explain client-side vs server-side optimizations during viva
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define PORT 8080
#define RECV_BUFFER_SIZE 65536 // 64KB buffer for receiving data
#define NUM_FIELDS 8

typedef struct {
    int thread_id;
    int msg_size;
    int duration;
    const char* server_ip;
    long* total_bytes_received;
    long* total_latency_us;
    long* total_recvs;
} client_thread_args_t;

void* run_client(void* args) {
    client_thread_args_t* thread_args = (client_thread_args_t*)args;
    int duration = thread_args->duration;
    long* total_bytes_received = thread_args->total_bytes_received;
    
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        pthread_exit(NULL);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    // User specified 10.0.0.2 as server IP for client connection
    if (inet_pton(AF_INET, thread_args->server_ip, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        close(sock);
        pthread_exit(NULL);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        close(sock);
        pthread_exit(NULL);
    }
    
    char ready_signal = 'R';
    if (send(sock, &ready_signal, 1, 0) <= 0) {
        perror("Handshake send failed");
        close(sock);
        pthread_exit(NULL);
    }

    char go_signal;
    if (recv(sock, &go_signal, 1, 0) <= 0) {
        perror("Handshake recv failed");
        close(sock);
        pthread_exit(NULL);
    }

    // Allocate buffer for receiving data
    char* recv_buffer = (char*)malloc(RECV_BUFFER_SIZE);
    if (!recv_buffer) {
        perror("Failed to allocate receive buffer");
        close(sock);
        pthread_exit(NULL);
    }

    struct timeval start_time, current_time, recv_start, recv_end;
    gettimeofday(&start_time, NULL);

    long bytes_this_thread = 0;
    long latency_this_thread = 0;
    long recvs_this_thread = 0;

    while (1) {
        gettimeofday(&current_time, NULL);
        if (current_time.tv_sec - start_time.tv_sec >= duration) {
            break;
        }

        gettimeofday(&recv_start, NULL);
        ssize_t bytes_received = recv(sock, recv_buffer, RECV_BUFFER_SIZE, 0);
        gettimeofday(&recv_end, NULL);

        if (bytes_received <= 0) {
            break;
        }
        bytes_this_thread += bytes_received;
        latency_this_thread += (recv_end.tv_sec - recv_start.tv_sec) * 1000000 + (recv_end.tv_usec - recv_start.tv_usec);
        recvs_this_thread++;
    }

    __sync_fetch_and_add(total_bytes_received, bytes_this_thread);
    __sync_fetch_and_add(thread_args->total_latency_us, latency_this_thread);
    __sync_fetch_and_add(thread_args->total_recvs, recvs_this_thread);
    
    free(recv_buffer);
    close(sock);
    return NULL;
}


int main(int argc, char const *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_ip> <thread_count> <message_size> <duration_in_seconds>\n", argv[0]);
        return 1;
    }

    const char* server_ip = argv[1];
    int thread_count = atoi(argv[2]);
    int msg_size = atoi(argv[3]);
    int duration = atoi(argv[4]);

    if (thread_count <= 0 || msg_size <= 0 || duration <= 0) {
        fprintf(stderr, "Invalid arguments. All values must be positive integers.\n");
        return 1;
    }
    if (msg_size % NUM_FIELDS != 0) {
        fprintf(stderr, "Message size (%d) must be divisible by NUM_FIELDS (%d) for this implementation.\n", msg_size, NUM_FIELDS);
        return 1;
    }

    printf("Starting %d client receiver threads...\n", thread_count);
    
    pthread_t* threads = (pthread_t*)malloc(thread_count * sizeof(pthread_t));
    client_thread_args_t* thread_args = (client_thread_args_t*)malloc(thread_count * sizeof(client_thread_args_t));
    long total_bytes_received = 0;
    long total_latency_us = 0;
    long total_recvs = 0;

    struct timeval start_test, end_test;
    gettimeofday(&start_test, NULL);

    for (int i = 0; i < thread_count; i++) {
        thread_args[i].thread_id = i;
        thread_args[i].server_ip = server_ip;
        thread_args[i].msg_size = msg_size;
        thread_args[i].duration = duration;
        thread_args[i].total_bytes_received = &total_bytes_received;
        thread_args[i].total_latency_us = &total_latency_us;
        thread_args[i].total_recvs = &total_recvs;

        if (pthread_create(&threads[i], NULL, run_client, &thread_args[i]) != 0) {
            perror("Failed to create thread");
        }
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    gettimeofday(&end_test, NULL);

    double elapsed_sec = (end_test.tv_sec - start_test.tv_sec) + 
                         (end_test.tv_usec - start_test.tv_usec) / 1000000.0;

    double total_bits = (double)total_bytes_received * 8.0;
    double throughput_gbps = 0.0;
    if (elapsed_sec > 0.000001) {
        throughput_gbps = (total_bits / elapsed_sec) / 1e9;
    }

    double avg_latency_us = 0.0;
    if (total_recvs > 0) {
        avg_latency_us = (double)total_latency_us / total_recvs;
    }
    
    printf("\nTest complete.\n");
    printf("Total bytes received: %ld\n", total_bytes_received);
    printf("Test Duration (Actual): %.6f seconds\n", elapsed_sec);
    printf("Throughput: %.6f Gbps\n", throughput_gbps);
    printf("Average Latency: %.6f us\n", avg_latency_us);
    
    free(threads);
    free(thread_args);

    return 0;
}
