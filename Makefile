# MT25043

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lpthread

# Source files
A1_SERVER_SRC = MT25043_Part_A1_Server.c
A1_CLIENT_SRC = MT25043_Part_A1_Client.c
A2_SERVER_SRC = MT25043_Part_A2_Server.c
A2_CLIENT_SRC = MT25043_Part_A2_Client.c
A3_SERVER_SRC = MT25043_Part_A3_Server.c
A3_CLIENT_SRC = MT25043_Part_A3_Client.c

# Executable names
A1_SERVER_EXE = two_copy_server
A1_CLIENT_EXE = two_copy_client
A2_SERVER_EXE = one_copy_server
A2_CLIENT_EXE = one_copy_client
A3_SERVER_EXE = zero_copy_server
A3_CLIENT_EXE = zero_copy_client

# Target groups
TARGETS = $(A1_SERVER_EXE) $(A1_CLIENT_EXE) $(A2_SERVER_EXE) $(A2_CLIENT_EXE) $(A3_SERVER_EXE) $(A3_CLIENT_EXE)

.PHONY: all clean

# Default target: build all executables
all: $(TARGETS)

# --- Build Rules ---

# Rule for Two-Copy (A1)
$(A1_SERVER_EXE): $(A1_SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(A1_CLIENT_EXE): $(A1_CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Rule for One-Copy (A2)
$(A2_SERVER_EXE): $(A2_SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(A2_CLIENT_EXE): $(A2_CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Rule for Zero-Copy (A3)
$(A3_SERVER_EXE): $(A3_SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(A3_CLIENT_EXE): $(A3_CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# --- Cleanup Rule ---
clean:
	rm -f $(TARGETS)

# --- Informational ---
info:
	@echo "Available targets:"
	@echo "  all     - Build all client/server executables (default)"
	@echo "  clean   - Remove all built executables"
	@echo "Executables created:"
	@echo "  $(A1_SERVER_EXE), $(A1_CLIENT_EXE)"
	@echo "  $(A2_SERVER_EXE), $(A2_CLIENT_EXE)"
	@echo "  $(A3_SERVER_EXE), $(A3_CLIENT_EXE)"
