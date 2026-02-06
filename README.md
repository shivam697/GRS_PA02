# MT25043 - Programming Assignment 02: Socket Communication Performance Analysis

## Overview

This project implements and compares three socket communication approaches to analyze their performance characteristics:
1. **Two-Copy (Baseline)**: Traditional `send()`/`recv()` with buffer copy
2. **One-Copy**: `sendmsg()` with scatter-gather I/O using `iovec`
3. **Zero-Copy**: `sendmsg()` with `MSG_ZEROCOPY` flag

The assignment evaluates throughput, latency, CPU cycles, cache behavior, and context switches across different message sizes and thread counts.

---

## Table of Contents

- [Project Structure](#project-structure)
- [System Requirements](#system-requirements)
- [Quick Start](#quick-start)
- [Implementation Details](#implementation-details)
- [Usage Instructions](#usage-instructions)
- [Performance Metrics](#performance-metrics)
- [Understanding Results](#understanding-results)
- [Troubleshooting](#troubleshooting)
- [AI Usage Declaration](#ai-usage-declaration)

---

## Project Structure

```
MT25043_PA02/
├── README.md                        # This file
├── AI_USAGE.md                      # Detailed AI usage declaration
├── Makefile                         # Build configuration
│
├── Part A: Socket Implementations
│   ├── MT25043_Part_A1_Server.c    # Two-copy server (sender)
│   ├── MT25043_Part_A1_Client.c    # Two-copy client (receiver)
│   ├── MT25043_Part_A2_Server.c    # One-copy server (sendmsg with iovec)
│   ├── MT25043_Part_A2_Client.c    # One-copy client (receiver)
│   ├── MT25043_Part_A3_Server.c    # Zero-copy server (MSG_ZEROCOPY)
│   └── MT25043_Part_A3_Client.c    # Zero-copy client (receiver)
│
├── Part C: Experiment Automation
│   ├── MT25043_Part_C_Script.sh    # Automated experiment runner
│   └── MT25043_Part_C_Results.csv  # Performance data (generated)
│
└── Part D: Visualization
    ├── MT25043_Part_D_Plotting.py  # Plot generation script
    ├── throughput_vs_msg_size.png  # Generated plots
    ├── latency_vs_thread_count.png
    ├── cache_misses_vs_msg_size.png
    └── cpu_cycles_per_byte.png
```

---

## System Requirements

### Software Dependencies
- **OS**: Linux (Ubuntu 20.04+ recommended)
- **Kernel**: 4.14+ (for MSG_ZEROCOPY support)
- **Compiler**: GCC 7.0+
- **Tools**:
  - `make` (build system)
  - `perf` (performance profiling)
  - `python3` (3.6+) with matplotlib
  - `sudo` access (for network namespaces)

### Hardware Requirements
- Multi-core CPU (for thread scaling tests)
- Minimum 4GB RAM
- Network namespace support in kernel

### Install Dependencies (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y build-essential linux-tools-generic python3-matplotlib
```

---

## Quick Start

### 1. Compile All Implementations
```bash
make clean
make all
```

**Expected Output:**
```
gcc -Wall -Wextra -O2 -o two_copy_server MT25043_Part_A1_Server.c -lpthread
gcc -Wall -Wextra -O2 -o two_copy_client MT25043_Part_A1_Client.c -lpthread
gcc -Wall -Wextra -O2 -o one_copy_server MT25043_Part_A2_Server.c -lpthread
gcc -Wall -Wextra -O2 -o one_copy_client MT25043_Part_A2_Client.c -lpthread
gcc -Wall -Wextra -O2 -o zero_copy_server MT25043_Part_A3_Server.c -lpthread
gcc -Wall -Wextra -O2 -o zero_copy_client MT25043_Part_A3_Client.c -lpthread
```

### 2. Run Automated Experiments
```bash
sudo ./MT25043_Part_C_Script.sh
```

**Duration**: ~15 minutes (48 experiments)  
**Output**: `MT25043_Part_C_Results.csv`

### 3. Generate Plots
```bash
python3 MT25043_Part_D_Plotting.py
```

**Output**: 4 PNG files with performance graphs

---

## Implementation Details

### Part A1: Two-Copy Implementation (Baseline)

**Server** ([MT25043_Part_A1_Server.c](MT25043_Part_A1_Server.c)):
- Creates 8 heap-allocated string fields per message
- Copies all fields into single buffer via `memcpy()`
- Sends buffer using `send()` system call
- **Copy Operations**: 2 (user→buffer, buffer→kernel)

**Client** ([MT25043_Part_A1_Client.c](MT25043_Part_A1_Client.c)):
- Receives data using `recv()` into pre-allocated buffer
- Measures throughput (Gbps) and latency (µs)

**Key Functions:**
```c
message_t* create_message(int field_size);  // Allocates 8 fields
void handle_client(void* args);             // Sends data repeatedly
```

---

### Part A2: One-Copy Implementation

**Server** ([MT25043_Part_A2_Server.c](MT25043_Part_A2_Server.c)):
- Uses `sendmsg()` with scatter-gather I/O
- Configures `iovec` array pointing to 8 separate buffers
- Kernel assembles message from multiple buffers
- **Copy Operations**: 1 (buffers→kernel, no intermediate copy)

**Client** ([MT25043_Part_A2_Client.c](MT25043_Part_A2_Client.c)):
- Standard `recv()` for receiving data

**Key Optimization:**
```c
struct iovec iov[NUM_FIELDS];
for (int i = 0; i < NUM_FIELDS; i++) {
    iov[i].iov_base = msg->field[i];  // Direct pointer
    iov[i].iov_len = field_size;
}
sendmsg(client_socket, &msg_hdr, 0);
```

---

### Part A3: Zero-Copy Implementation

**Server** ([MT25043_Part_A3_Server.c](MT25043_Part_A3_Server.c)):
- Uses `sendmsg()` with `MSG_ZEROCOPY` flag
- Kernel references user buffers via DMA
- Drains error queue (`MSG_ERRQUEUE`) for completion notifications
- **Copy Operations**: 0 (kernel uses pointers until NIC DMA complete)

**Client** ([MT25043_Part_A3_Client.c](MT25043_Part_A3_Client.c)):
- Standard `recv()` (zero-copy is sender-side optimization)

**Key Features:**
```c
setsockopt(sock, SOL_SOCKET, SO_ZEROCOPY, &opt, sizeof(opt));
sendmsg(client_socket, &msg_hdr, MSG_ZEROCOPY);
recvmsg(sock, &r_msg_hdr, MSG_ERRQUEUE | MSG_DONTWAIT);  // Completion
```

---

### Part C: Automated Experiment Script

**Script** ([MT25043_Part_C_Script.sh](MT25043_Part_C_Script.sh)):

1. **Setup Phase**:
   - Creates network namespaces (`ns1` for server, `ns2` for client)
   - Establishes virtual ethernet pair (`veth-ns1` ↔ `veth-ns2`)
   - Configures IP addressing (10.0.1.1 ↔ 10.0.1.2)

2. **Execution Phase**:
   - Runs 48 experiments (3 implementations × 4 sizes × 4 threads)
   - Message sizes: 1KB, 4KB, 16KB, 64KB
   - Thread counts: 1, 2, 4, 8
   - Duration: 10 seconds per experiment

3. **Profiling**:
   - Wraps client in `perf stat` for metrics:
     - CPU cycles, instructions, branches
     - L1 data cache misses, LLC (last-level cache) misses
     - Branch misses, context switches
   - Parses client output for throughput and latency

4. **Cleanup**:
   - Automatic namespace deletion via trap on exit

**CSV Output Format:**
```
Implementation,Threads,MsgSize_Bytes,Duration_s,Throughput_Gbps,Latency_us,
Cycles,Instructions,L1_Cache_Misses,LLC_Misses,Branches,Branch_Misses,Context_Switches
```

---

### Part D: Visualization

**Script** ([MT25043_Part_D_Plotting.py](MT25043_Part_D_Plotting.py)):

Generates 4 comparative plots using matplotlib:

1. **Throughput vs Message Size** (4 threads fixed)
   - Shows how throughput scales with message size
   - Log scale for X-axis (powers of 2)

2. **Latency vs Thread Count** (16KB message fixed)
   - Analyzes threading impact on latency
   - Linear scale to show concurrency effects

3. **Cache Misses vs Message Size** (4 threads fixed)
   - Log-log scale for wide value ranges
   - Separates L1 and LLC misses

4. **CPU Cycles per Byte** (4 threads fixed)
   - Efficiency metric: lower is better
   - Formula: `cycles_per_byte = total_cycles / message_size`

**All plots include:**
- Clear axis labels with units
- Implementation legends
- System configuration footer
- Publication-quality 10×6 inch figures

---

## Usage Instructions

### Manual Testing (Single Experiment)

**Terminal 1: Start Server**
```bash
./two_copy_server 8192 10
# Arguments: <message_size> <duration_seconds>
```

**Terminal 2: Run Client**
```bash
./two_copy_client 127.0.0.1 4 8192 10
# Arguments: <server_ip> <threads> <message_size> <duration>
```

**Expected Output (Client):**
```
Starting 4 client receiver threads...
Test complete.
Total bytes received: 42949672960
Test Duration (Actual): 10.000156 seconds
Throughput: 34.359738 Gbps
Average Latency: 9.324629 us
```

---

### Running Individual Implementations

**Two-Copy (A1):**
```bash
./two_copy_server 16384 5 &
./two_copy_client 127.0.0.1 2 16384 5
```

**One-Copy (A2):**
```bash
./one_copy_server 4096 5 &
./one_copy_client 127.0.0.1 1 4096 5
```

**Zero-Copy (A3):**
```bash
./zero_copy_server 65536 5 &
./zero_copy_client 127.0.0.1 8 65536 5
```

---

### Full Experiment Suite

**Option 1: Run everything automatically**
```bash
sudo ./MT25043_Part_C_Script.sh
```

**Option 2: Step-by-step**
```bash
# 1. Compile
make clean && make all

# 2. Run experiments
sudo ./MT25043_Part_C_Script.sh

# 3. Generate plots
python3 MT25043_Part_D_Plotting.py

# 4. View results
cat MT25043_Part_C_Results.csv
xdg-open throughput_vs_msg_size.png
```

---

## Performance Metrics

### Collected Metrics

| Metric | Unit | Description |
|--------|------|-------------|
| Throughput | Gbps | Total bits transferred per second |
| Latency | µs | Average time per send/recv operation |
| CPU Cycles | count | Total processor cycles consumed |
| Instructions | count | Total CPU instructions executed |
| L1 Cache Misses | count | L1 data cache load misses |
| LLC Misses | count | Last-level cache load misses |
| Branches | count | Total branch instructions |
| Branch Misses | count | Mispredicted branches |
| Context Switches | count | Kernel context switches |

### Interpreting Results

**Throughput:**
- Higher is better
- Expected: Zero-copy ≥ One-copy > Two-copy (for large messages)
- Saturation at high thread counts indicates bottleneck

**Latency:**
- Lower is better
- Increases with thread count due to contention
- Two-copy has extra overhead from buffer copy

**Cache Misses:**
- Lower is better
- Increases with message size (working set exceeds cache)
- Zero-copy may show higher misses (no data copy brings data to cache)

**CPU Cycles per Byte:**
- Lower = more efficient
- Expected: Zero-copy < One-copy < Two-copy
- Decreases with message size (amortized overhead)

---

## Troubleshooting

### Common Issues

**1. Permission Denied (Network Namespaces)**
```
Error: Operation not permitted
Solution: Run script with sudo
```

**2. MSG_ZEROCOPY Not Supported**
```
setsockopt(SO_ZEROCOPY) failed
Solution: Upgrade kernel to 4.14+, or results show "one-copy" behavior
```

**3. Perf Not Found**
```
perf: command not found
Solution: sudo apt install linux-tools-$(uname -r)
```

**4. Python Import Error**
```
ModuleNotFoundError: No module named 'matplotlib'
Solution: pip3 install matplotlib
```

**5. Compilation Warnings**
```
warning: unused variable 'msg_size'
Solution: Already fixed - recompile with make clean && make all
```

**6. Server Connection Refused**
```
Connection Failed
Solution: Ensure server is running first, check firewall settings
```

---

## Understanding the Architecture

### Server Role (Changed from Original)
- **Sends** structured messages with 8 heap-allocated fields
- Accepts command-line parameters: `<message_size> <duration>`
- Uses different sending methods per implementation
- Waits for client "Ready" signal before starting

### Client Role
- **Receives** data and measures performance
- Accepts parameters: `<server_ip> <threads> <message_size> <duration>`
- Creates multiple threads for concurrent connections
- Aggregates throughput/latency across threads

### Handshake Protocol
1. Client connects to server
2. Client sends 'R' (Ready) signal
3. Server sends 'G' (Go) signal
4. Data transfer begins
5. After `duration` seconds, client disconnects

---

## Experimental Design

### Parameters
- **Message Sizes**: 1024, 4096, 16384, 65536 bytes
- **Thread Counts**: 1, 2, 4, 8
- **Duration**: 10 seconds per experiment
- **Total Experiments**: 3 implementations × 4 sizes × 4 threads = 48

### Controlled Variables
- Network: Isolated namespaces (10.0.1.1 ↔ 10.0.1.2)
- Duration: Fixed 10 seconds
- Field count: Always 8 fields per message

### Why Network Namespaces?
- **Isolation**: No interference from other network traffic
- **Reproducibility**: Consistent network conditions
- **Control**: Predictable latency and bandwidth
- **Fair Comparison**: All implementations tested identically

---

## AI Usage Declaration

This project used GitHub Copilot for assistance with:
- Socket API boilerplate and error handling
- Bash script structure and perf parsing
- Matplotlib plotting syntax and documentation

**Detailed AI usage declarations are included as comments in each source file:**
- **C files**: Lines 9-39 (comprehensive header comments)
- **Script**: Lines 21-48 (bash comments)
- **Python**: Lines 13-42 (docstring)

**Student Accountability**: 
- All code is understood and can be explained line-by-line
- Experimental design and analysis are original work
- Ready to explain implementation details, performance implications, and design decisions during viva

**Key Understanding Points**:
- Socket programming: send/recv vs sendmsg, MSG_ZEROCOPY mechanics
- Performance metrics: throughput, latency, cache behavior, CPU efficiency
- Experimental design: network namespaces, perf stat, metric interpretation
- Visualization: log vs linear scales, plot design choices

---

## References

### Documentation Used
- Linux man pages: `socket(2)`, `send(2)`, `sendmsg(2)`, `perf-stat(1)`
- MSG_ZEROCOPY: https://www.kernel.org/doc/html/latest/networking/msg_zerocopy.html
- Network namespaces: `ip-netns(8)`

### Key Concepts
- **Two-copy**: User space → buffer → kernel
- **One-copy**: Scatter-gather I/O eliminates intermediate buffer
- **Zero-copy**: DMA from user pages, async completion via errqueue

---

## Author

**Student ID**: MT25043  
**Course**: CSE638 - Advanced Computer Networks  
**Assignment**: Programming Assignment 02  
**Date**: February 2026

---

## License

Academic use only. Not for redistribution.

---

## Appendix: File Responsibilities

| File | Lines | Purpose | AI Assistance |
|------|-------|---------|---------------|
| MT25043_Part_A1_Server.c | 189 | Two-copy sender | Boilerplate |
| MT25043_Part_A1_Client.c | 187 | Two-copy receiver | Socket setup |
| MT25043_Part_A2_Server.c | 186 | One-copy sender | sendmsg() syntax |
| MT25043_Part_A2_Client.c | 191 | One-copy receiver | Performance |
| MT25043_Part_A3_Server.c | 214 | Zero-copy sender | MSG_ZEROCOPY |
| MT25043_Part_A3_Client.c | 191 | Zero-copy receiver | Rewrite |
| MT25043_Part_C_Script.sh | 196 | Experiment runner | Bash structure |
| MT25043_Part_D_Plotting.py | 397 | Visualization | Matplotlib |
| AI_USAGE.md | 148 | AI declaration | Manual |
| README.md | This file | Documentation | Manual |

**Total**: ~2,100 lines of code and documentation
