#!/usr/bin/env python3
"""
MT25043 - Programming Assignment 02 - Part D: Plotting and Visualization

This script generates performance comparison plots for three socket communication implementations:
1. Two-Copy (send/recv with intermediate buffer)
2. One-Copy (sendmsg with iovec scatter-gather)
3. Zero-Copy (sendmsg with MSG_ZEROCOPY)

All experimental data is hardcoded (not read from CSV) as per assignment requirements.

================================================================================
AI USAGE DECLARATION
================================================================================
Tool: GitHub Copilot

Components Where AI Assisted:
- Matplotlib boilerplate and figure setup code
- Plot customization syntax (log scales, grid, legend placement)
- Comprehensive inline comments and docstrings
- Data structure organization (nested dictionaries)

Prompts Used:
- "Create plotting functions for throughput vs message size"
- "Add comments to explain matplotlib parameters"
- "Format data from CSV into Python arrays"
- "Use logarithmic scale for x and y axes"
- "Add system configuration text to plot footer"

Student Understanding:
- Understands matplotlib workflow: figure -> plot -> customize -> save
- Understands when to use log vs linear scales:
  * Throughput plot: log X-axis for powers-of-2 message sizes
  * Latency plot: linear scales to show threading impact
  * Cache misses: log-log for wide value ranges (orders of magnitude)
  * CPU cycles/byte: log-log for efficiency comparison
- Understands numpy operations for calculations (cycles_per_byte)
- Can explain why 4 threads chosen for most plots (mid-range concurrency)
- Can explain matplotlib.use('Agg'): non-interactive backend for headless
- Can explain data hardcoding: reproducibility without external files
- Ready to explain plot design choices and metric interpretations during viva

Data Source:
- All 528 values (48 experiments × 11 metrics) from actual system runs
- No synthetic or AI-generated data
- Represents real measurements from MT25043_Part_C_Results.csv
================================================================================

Author: MT25043
Date: February 2026
"""

# ============================================================================
# Library Imports
# ============================================================================

import matplotlib
matplotlib.use('Agg')  # Use non-interactive backend for headless plot generation
import matplotlib.pyplot as plt
import numpy as np

# ============================================================================
# System Configuration
# ============================================================================

# System configuration displayed on all plots
# TODO: Update with your actual system specifications
SYSTEM_CONFIG = """
CPU: Intel Core i7-8750H @ 2.20GHz
Cache: L1d 32K, L1i 32K, L2 256K, L3 9216K
RAM: 16GB
OS: Ubuntu 22.04.3 LTS (on WSL2)
Kernel: 5.15.90.1-microsoft-standard-WSL2
GCC: 11.4.0
"""

# ============================================================================
# Hardcoded Experimental Data
# ============================================================================

# All data is hardcoded from MT25043_Part_C_Results.csv as per assignment requirements
# Each implementation has 16 data points (4 message sizes × 4 thread counts)
# Data structure: Dictionary -> Implementation -> Metric -> Array of values

data = {
    # Two-Copy Implementation: Traditional recv() and send() with buffer copy
    'two_copy': {
        'threads': [1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 8, 8, 8, 8],
        'msg_size': [1024, 4096, 16384, 65536, 1024, 4096, 16384, 65536, 1024, 4096, 16384, 65536, 1024, 4096, 16384, 65536],
        'duration': [10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10],
        'throughput': [1.330161, 3.967894, 13.980010, 27.197132, 2.297739, 6.283767, 20.531938, 38.296060, 2.625095, 7.030993, 22.306982, 43.902117, 3.323042, 9.388556, 27.866117, 43.679302],
        'latency': [6.106586, 8.206266, 9.324629, 19.223828, 7.068908, 10.362168, 12.701335, 27.307123, 12.371866, 18.530874, 23.387225, 47.650630, 19.595317, 27.787847, 37.458576, 95.848492],
        'cpu_cycles': [22758370135, 23979066082, 24208362522, 24343126182, 42127350308, 38080862478, 38779605333, 35606380847, 69312673004, 69404598468, 69663021914, 62209727106, 92279435607, 91865686895, 81745956779, 72179164312],
        'instructions': [21489179381, 23381077356, 19934367895, 17883592165, 38796684464, 35995178714, 31441477336, 24851252612, 41259077965, 41376598207, 38816128610, 28584223235, 56720981827, 57002801050, 41213201538, 22869757541],
        'cache_misses': [2326676, 1907807, 1203686, 1741771, 2100249, 2262948, 2088386, 2575626, 3817344, 8678243, 25973959, 41239234, 44505967, 122632400, 468128002, 857849428],
        'branches': [3535239145, 3838972695, 3285398272, 3009458949, 6381965577, 5909479065, 5189933104, 4187362382, 6782404375, 6796295502, 6403519263, 4812001627, 9291441784, 9311192811, 6749049972, 3825752587],
        'branch_misses': [70538371, 76339507, 60716655, 49465782, 127553126, 117444852, 98245943, 68644103, 149698593, 147822798, 135444316, 83344815, 188183219, 189955985, 139720174, 69224696],
        'context_switches': [1520, 327, 203, 222, 506, 582, 586, 191, 7825, 11826, 7697, 4304, 717280, 654900, 487281, 398227],
    },
    
    # One-Copy Implementation: Using MSG_TRUNC to reduce one copy operation
    'one_copy': {
        'threads': [1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 8, 8, 8, 8],
        'msg_size': [1024, 4096, 16384, 65536, 1024, 4096, 16384, 65536, 1024, 4096, 16384, 65536, 1024, 4096, 16384, 65536],
        'duration': [10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10],
        'throughput': [1.196990, 3.153054, 11.166201, 23.390240, 1.864384, 4.940816, 16.683962, 35.509795, 2.330034, 6.004763, 19.975341, 46.893808, 2.620820, 8.953111, 26.210986, 41.472874],
        'latency': [6.791111, 10.332512, 11.679266, 22.355093, 8.712677, 13.182161, 15.632153, 29.450903, 13.946929, 21.710354, 26.132631, 44.593974, 24.895879, 29.154814, 39.841064, 100.916577],
        'cpu_cycles': [23717037754, 21051870562, 21541739128, 21622275231, 33958140629, 33775082789, 33853181989, 34203952575, 67509080175, 67910040378, 67097471854, 67233487800, 90675207369, 94951414952, 80537914206, 67100394391],
        'instructions': [22588866085, 20780531902, 18694703381, 16149098019, 29552317661, 31819295855, 27279424922, 23429220922, 40520057126, 39876048803, 38631923113, 30622501956, 53633248531, 58156455921, 41410258683, 24824546314],
        'cache_misses': [534206, 533378, 577624, 971809, 1198592, 1191388, 1817328, 4136797, 3901912, 5396950, 15503925, 68322464, 22919291, 115286773, 400438801, 753261835],
        'branches': [3767704215, 3446347341, 3110400894, 2738104485, 4928758793, 5277298806, 4543128843, 3968936079, 6756450176, 6614468175, 6423696353, 5187622168, 8934408427, 9599014740, 6858439982, 4183151076],
        'branch_misses': [71110796, 67316529, 57610601, 43047551, 93904477, 103149147, 85599084, 65113507, 141938989, 141396310, 131963547, 89098692, 164896015, 190839230, 135976954, 75890266],
        'context_switches': [369, 165, 245, 142, 397, 494, 683, 275, 6978, 7239, 8304, 3250, 905005, 681883, 516751, 480001],
    },
    
    # Zero-Copy Implementation: Using splice() system call for kernel-level data transfer
    'zero_copy': {
        'threads': [1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 8, 8, 8, 8],
        'msg_size': [1024, 4096, 16384, 65536, 1024, 4096, 16384, 65536, 1024, 4096, 16384, 65536, 1024, 4096, 16384, 65536],
        'duration': [10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10],
        'throughput': [0.379553, 1.249684, 3.432303, 8.872064, 0.720760, 2.291321, 6.970915, 16.497670, 0.912258, 2.742203, 9.124664, 22.906465, 1.004202, 2.699432, 9.498833, 8.735863],
        'latency': [15.341248, 17.714234, 23.133908, 47.875988, 16.001061, 19.647585, 24.191750, 48.359126, 25.673157, 33.409755, 38.344806, 65.544446, 54.087076, 83.064790, 94.586326, 124.140046],
        'cpu_cycles': [15688797235, 15708949439, 15547759576, 15513631288, 31343390443, 31269125949, 31209793626, 30845042087, 61247035412, 60329529645, 60910091760, 60730923846, 68187111695, 83930803219, 84991165508, 44473920319],
        'instructions': [16489576149, 16335609808, 16122144849, 16972028595, 30603628203, 30703418356, 31994001556, 32700600930, 39148241938, 37116923581, 44100681177, 45873594139, 39396003706, 48763447354, 51758419381, 27706376262],
        'cache_misses': [4073030, 7649031, 21391443, 4387198, 7582039, 13152514, 30751921, 23762973, 10558892, 28336239, 52036339, 53856793, 17751539, 15495219, 15366466, 196822245],
        'branches': [2736024977, 2664162750, 2580687980, 2527644930, 5081348017, 5011658169, 5117384379, 4907085863, 6506132901, 6060902259, 7069032362, 6900383821, 6558107437, 7990593647, 8310208029, 4363271128],
        'branch_misses': [34333955, 33938555, 28159243, 25105792, 65669959, 67177250, 57667548, 50202374, 115013667, 110562834, 109468963, 86539689, 98557833, 118844643, 110756395, 56001053],
        'context_switches': [670, 447, 1946, 217, 1082, 1113, 980, 318, 10921, 26826, 17183, 2709, 636158, 739803, 695380, 87034],
    }
}

# ============================================================================
# Plotting Functions
# ============================================================================

def plot_throughput_vs_msg_size(data):
    """
    Generate plot: Throughput vs. Message Size
    
    Purpose: Compare how throughput scales with increasing message sizes
    Fixed Parameter: 4 threads (moderate concurrency)
    Variable: Message size (1KB, 4KB, 16KB, 64KB)
    
    Args:
        data (dict): Dictionary containing experimental data for all implementations
        
    Output:
        throughput_vs_msg_size.png - Saved to current directory
    """
    # Create figure with specific size (10x6 inches)
    plt.figure(figsize=(10, 6))
    
    # Select thread count to analyze (4 threads chosen for mid-range concurrency)
    thread_count_to_plot = 4
    
    # Iterate through each implementation (two_copy, one_copy, zero_copy)
    for impl, metrics in data.items():
        # Filter data: Select only rows where thread count matches our target
        # List comprehension finds all indices where threads == 4
        indices = [i for i, t in enumerate(metrics['threads']) if t == thread_count_to_plot]
        
        # Extract message sizes and throughputs for these filtered indices
        msg_sizes = [metrics['msg_size'][i] for i in indices]
        throughputs = [metrics['throughput'][i] for i in indices]
        
        # Plot line with markers: 'o' = circle markers, '-' = solid line
        plt.plot(msg_sizes, throughputs, marker='o', linestyle='-', label=impl)

    # Set plot title with dynamic thread count value
    plt.title(f'Throughput vs. Message Size ({thread_count_to_plot} Threads)')
    
    # Label X-axis (Message Size in Bytes)
    plt.xlabel('Message Size (Bytes)')
    
    # Label Y-axis (Throughput in Gigabits per second)
    plt.ylabel('Throughput (Gbps)')
    
    # Use logarithmic scale for X-axis (better visualization for powers of 2)
    plt.xscale('log')
    
    # Add grid for easier reading (both major and minor grid lines)
    plt.grid(True, which="both", ls="--")
    
    # Add legend to identify different implementations
    plt.legend()
    
    # Add system configuration as footer text (bottom right corner)
    plt.figtext(0.99, 0.01, SYSTEM_CONFIG, horizontalalignment='right', 
                fontsize=8, multialignment='left', linespacing=1.5)
    
    # Adjust layout to prevent text overlap (leave space for footer)
    plt.tight_layout(rect=[0, 0.1, 1, 0.95])
    
    # Save plot to PNG file
    plt.savefig('throughput_vs_msg_size.png')
    
    # Close figure to free memory
    plt.close()


def plot_latency_vs_thread_count(data):
    """
    Generate plot: Latency vs. Thread Count
    
    Purpose: Analyze how latency changes with increasing thread concurrency
    Fixed Parameter: 16KB message size (mid-range message size)
    Variable: Thread count (1, 2, 4, 8)
    
    Args:
        data (dict): Dictionary containing experimental data for all implementations
        
    Output:
        latency_vs_thread_count.png - Saved to current directory
    """
    # Create new figure
    plt.figure(figsize=(10, 6))
    
    # Select message size to analyze (16KB chosen as mid-range)
    msg_size_to_plot = 16384  # 16KB in bytes
    
    # Iterate through each implementation
    for impl, metrics in data.items():
        # Filter data: Select only rows where message size matches target
        indices = [i for i, s in enumerate(metrics['msg_size']) if s == msg_size_to_plot]
        
        # Extract thread counts and latencies for filtered indices
        threads = [metrics['threads'][i] for i in indices]
        latencies = [metrics['latency'][i] for i in indices]
        
        # Plot with markers and connecting lines
        plt.plot(threads, latencies, marker='o', linestyle='-', label=impl)

    # Set descriptive title
    plt.title(f'Latency vs. Thread Count (Message Size: {msg_size_to_plot} Bytes)')
    
    # Label axes
    plt.xlabel('Number of Threads')
    plt.ylabel('Average Latency (µs)')  # µs = microseconds
    
    # Set X-axis ticks to show all thread counts (1, 2, 4, 8)
    # Extract unique thread counts from data and sort them
    plt.xticks(sorted(list(set(data['two_copy']['threads']))))
    
    # Add grid for readability
    plt.grid(True, which="both", ls="--")
    
    # Add legend
    plt.legend()
    
    # Add system configuration footer
    plt.figtext(0.99, 0.01, SYSTEM_CONFIG, horizontalalignment='right', 
                fontsize=8, multialignment='left', linespacing=1.5)
    
    # Adjust layout
    plt.tight_layout(rect=[0, 0.1, 1, 0.95])
    
    # Save to file
    plt.savefig('latency_vs_thread_count.png')
    
    # Close figure
    plt.close()


def plot_cache_misses_vs_msg_size(data):
    """
    Generate plot: Cache Misses vs. Message Size
    
    Purpose: Analyze cache behavior with different message sizes
    Fixed Parameter: 4 threads
    Variable: Message size (1KB, 4KB, 16KB, 64KB)
    
    Args:
        data (dict): Dictionary containing experimental data for all implementations
        
    Output:
        cache_misses_vs_msg_size.png - Saved to current directory
    """
    # Create new figure
    plt.figure(figsize=(10, 6))

    # Use 4 threads for consistency with throughput plot
    thread_count_to_plot = 4

    # Process each implementation
    for impl, metrics in data.items():
        # Filter for selected thread count
        indices = [i for i, t in enumerate(metrics['threads']) if t == thread_count_to_plot]
        
        # Extract message sizes and cache miss counts
        msg_sizes = [metrics['msg_size'][i] for i in indices]
        cache_misses = [metrics['cache_misses'][i] for i in indices]

        # Plot data
        plt.plot(msg_sizes, cache_misses, marker='o', linestyle='-', label=impl)

    # Set title
    plt.title(f'Cache Misses vs. Message Size ({thread_count_to_plot} Threads)')
    
    # Label axes
    plt.xlabel('Message Size (Bytes)')
    plt.ylabel('Cache Misses')
    
    # Use log scale for both axes (wide range of values)
    plt.xscale('log')  # Message sizes: 1024 to 65536
    plt.yscale('log')  # Cache misses vary by orders of magnitude
    
    # Add grid
    plt.grid(True, which="both", ls="--")
    
    # Add legend
    plt.legend()

    # Add system configuration footer
    plt.figtext(0.99, 0.01, SYSTEM_CONFIG, horizontalalignment='right', 
                fontsize=8, multialignment='left', linespacing=1.5)
    
    # Adjust layout
    plt.tight_layout(rect=[0, 0.1, 1, 0.95])
    
    # Save plot
    plt.savefig('cache_misses_vs_msg_size.png')
    
    # Close figure
    plt.close()


def plot_cpu_cycles_per_byte(data):
    """
    Generate plot: CPU Cycles per Byte vs. Message Size
    
    Purpose: Measure CPU efficiency (lower is better)
    Fixed Parameter: 4 threads
    Variable: Message size (1KB, 4KB, 16KB, 64KB)
    
    Calculation: cycles_per_byte = total_cpu_cycles / message_size
    This metric shows how many CPU cycles are needed to transfer one byte.
    
    Args:
        data (dict): Dictionary containing experimental data for all implementations
        
    Output:
        cpu_cycles_per_byte.png - Saved to current directory
    """
    # Create new figure
    plt.figure(figsize=(10, 6))

    # Use 4 threads for consistency
    thread_count_to_plot = 4

    # Process each implementation
    for impl, metrics in data.items():
        # Filter for selected thread count
        indices = [i for i, t in enumerate(metrics['threads']) if t == thread_count_to_plot]
        
        # Extract and convert to numpy arrays for element-wise division
        msg_sizes = np.array([metrics['msg_size'][i] for i in indices])
        cpu_cycles = np.array([metrics['cpu_cycles'][i] for i in indices])
        
        # Calculate efficiency metric: CPU cycles per byte transferred
        # Lower values = more efficient (less CPU work per byte)
        cycles_per_byte = cpu_cycles / msg_sizes

        # Plot efficiency metric
        plt.plot(msg_sizes, cycles_per_byte, marker='o', linestyle='-', label=impl)

    # Set title
    plt.title(f'CPU Cycles per Byte vs. Message Size ({thread_count_to_plot} Threads)')
    
    # Label axes
    plt.xlabel('Message Size (Bytes)')
    plt.ylabel('CPU Cycles / Byte')
    
    # Use log scale for both axes
    plt.xscale('log')  # Message sizes span multiple orders of magnitude
    plt.yscale('log')  # Cycles per byte also vary significantly
    
    # Add grid
    plt.grid(True, which="both", ls="--")
    
    # Add legend
    plt.legend()
    
    # Add system configuration footer
    plt.figtext(0.99, 0.01, SYSTEM_CONFIG, horizontalalignment='right', 
                fontsize=8, multialignment='left', linespacing=1.5)
    
    # Adjust layout
    plt.tight_layout(rect=[0, 0.1, 1, 0.95])
    
    # Save plot
    plt.savefig('cpu_cycles_per_byte.png')
    
    # Close figure
    plt.close()


# ============================================================================
# Main Execution
# ============================================================================

if __name__ == '__main__':
    """
    Main entry point for plot generation.
    Generates all four required plots for the assignment.
    """
    print("Generating plots...")
    
    # Generate Plot 1: Throughput vs Message Size
    plot_throughput_vs_msg_size(data)
    print("✓ throughput_vs_msg_size.png generated")
    
    # Generate Plot 2: Latency vs Thread Count
    plot_latency_vs_thread_count(data)
    print("✓ latency_vs_thread_count.png generated")
    
    # Generate Plot 3: Cache Misses vs Message Size
    plot_cache_misses_vs_msg_size(data)
    print("✓ cache_misses_vs_msg_size.png generated")
    
    # Generate Plot 4: CPU Cycles per Byte
    plot_cpu_cycles_per_byte(data)
    print("✓ cpu_cycles_per_byte.png generated")
    
    print("\nAll plots generated successfully!")
    print("Output files: throughput_vs_msg_size.png, latency_vs_thread_count.png,")
    print("              cache_misses_vs_msg_size.png, cpu_cycles_per_byte.png")
