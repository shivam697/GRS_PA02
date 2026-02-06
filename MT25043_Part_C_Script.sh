#!/bin/bash

# MT25043
# Simplified fixed version

set -e

# Experimental Parameters
MESSAGE_SIZES=(1024 4096 16384 65536)
THREAD_COUNTS=(1 2 4 8)
DURATION=10

# Network Namespace Configuration
SERVER_NS="ns1"
CLIENT_NS="ns2"
VETH_SERVER="veth-ns1"
VETH_CLIENT="veth-ns2"
SERVER_IP="10.0.1.1"
CLIENT_IP="10.0.1.2"
SUBNET_MASK="24"

# Results File
RESULTS_FILE="MT25043_Part_C_Results.csv"

# Functions
cleanup() {
    echo "--- Cleaning up network namespaces ---"
    ip netns del "$SERVER_NS" &> /dev/null || true
    ip netns del "$CLIENT_NS" &> /dev/null || true
    echo "Cleanup complete."
}

setup_namespaces() {
    echo "--- Setting up network namespaces ---"
    ip netns add "$SERVER_NS"
    ip netns add "$CLIENT_NS"
    ip link add "$VETH_SERVER" type veth peer name "$VETH_CLIENT"
    ip link set "$VETH_SERVER" netns "$SERVER_NS"
    ip link set "$VETH_CLIENT" netns "$CLIENT_NS"
    ip netns exec "$SERVER_NS" ip addr add "${SERVER_IP}/${SUBNET_MASK}" dev "$VETH_SERVER"
    ip netns exec "$SERVER_NS" ip link set dev "$VETH_SERVER" up
    ip netns exec "$SERVER_NS" ip link set dev lo up
    ip netns exec "$CLIENT_NS" ip addr add "${CLIENT_IP}/${SUBNET_MASK}" dev "$VETH_CLIENT"
    ip netns exec "$CLIENT_NS" ip link set dev "$VETH_CLIENT" up
    ip netns exec "$CLIENT_NS" ip link set dev lo up
    echo "Pinging server from client to verify connectivity..."
    ip netns exec "$CLIENT_NS" ping -c 3 "$SERVER_IP"
    echo "Namespace setup complete."
}

# Main Script
if [[ $EUID -ne 0 ]]; then
   echo "This script requires root privileges. Please run with sudo."
   exit 1
fi

trap cleanup EXIT

echo "--- Compiling source code ---"
make clean
make all
echo "Compilation complete."

cleanup
setup_namespaces

echo "--- Preparing for experiments ---"
echo "Implementation,Threads,MsgSize_Bytes,Duration_s,Throughput_Gbps,Latency_us,Cycles,Instructions,L1_Cache_Misses,LLC_Misses,Branches,Branch_Misses,Context_Switches" > "$RESULTS_FILE"
echo "Results will be stored in $RESULTS_FILE"

for impl in two_copy one_copy zero_copy; do
    SERVER_EXE="${impl}_server"
    CLIENT_EXE="${impl}_client"

    for threads in "${THREAD_COUNTS[@]}"; do
        for size in "${MESSAGE_SIZES[@]}"; do
            echo "--- Running: Impl=$impl, Threads=$threads, Size=$size ---"

            ip netns exec "$SERVER_NS" ./"$SERVER_EXE" &
            SERVER_PID=$!
            sleep 1

            # Run client with perf - capture ALL output
            ALL_OUTPUT=$(ip netns exec "$CLIENT_NS" perf stat \
                -x, \
                -e cycles,instructions,L1-dcache-load-misses,LLC-load-misses,branches,branch-misses,context-switches \
                ./"$CLIENT_EXE" "$SERVER_IP" "$threads" "$size" "$DURATION" 2>&1)

            kill "$SERVER_PID" 2>/dev/null || true
            wait "$SERVER_PID" 2>/dev/null || true

            # Parse client output
            THROUGHPUT=$(echo "$ALL_OUTPUT" | grep "Throughput" | awk '{print $2}')
            LATENCY=$(echo "$ALL_OUTPUT" | grep "Average Latency" | awk '{print $3}')

            # Parse perf metrics (CSV format: value,,event_name,...)
            parse_metric() {
                local metric="$1"
                echo "$ALL_OUTPUT" | awk -F, -v m="$metric" '$3 ~ m {if ($1 ~ /^[0-9]+$/) print $1; else print "N/A"; exit}' | head -1
                if [[ -z "${PIPESTATUS[1]}" ]] || [[ "$(echo "$ALL_OUTPUT" | awk -F, -v m="$metric" '$3 ~ m {print}')" == "" ]]; then
                    echo "N/A"
                fi
            }

            CYCLES=$(parse_metric "cycles")
            INSTRUCTIONS=$(parse_metric "instructions")
            L1_CACHE_MISSES=$(parse_metric "L1-dcache-load-misses")
            LLC_MISSES=$(parse_metric "LLC-load-misses")
            BRANCHES=$(parse_metric "branches")
            BRANCH_MISSES=$(parse_metric "branch-misses")
            CONTEXT_SWITCHES=$(parse_metric "context-switches")

            # Default to N/A if empty
            THROUGHPUT=${THROUGHPUT:-"N/A"}
            LATENCY=${LATENCY:-"N/A"}
            CYCLES=${CYCLES:-"N/A"}
            L1_CACHE_MISSES=${L1_CACHE_MISSES:-"N/A"}
            LLC_MISSES=${LLC_MISSES:-"N/A"}
            CACHE_MISSES=${CACHE_MISSES:-"N/A"}
            BRANCHES=${BRANCHES:-"N/A"}
            BRANCH_MISSES=${BRANCH_MISSES:-"N/A"}
            CONTEXT_SWITCHES=${CONTEXT_SWITCHES:-"N/A"}

            echo "$impl,$threads,$size,$DURATION,$THROUGHPUT,$LATENCY,$CYCLES,$INSTRUCTIONS,$L1_CACHE_MISSES,$LLC_MISSES,$BRANCHES,$BRANCH_MISSES,$CONTEXT_SWITCHES" >> "$RESULTS_FILE"
            
            echo "TP: $THROUGHPUT Gbps, Lat: $LATENCY us, Cyc: $CYCLES, Inst: $INSTRUCTIONS"
            sleep 1
        done
    done
done

echo "--- All experiments complete ---"
exit 0
