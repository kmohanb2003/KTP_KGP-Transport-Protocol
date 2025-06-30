# KTP Socket Implementation: Reliable Transport Protocol over UDP

## Overview
This project implements a reliable transport protocol called **KTP (KGP Transport Protocol)** on top of UDP sockets. The protocol provides reliable, in-order message delivery using sliding window flow control and retransmission mechanisms. Key features include:

- **End-to-end reliability** over unreliable UDP channels
- **Sliding window flow control** with configurable window size
- **Selective retransmission** on timeout
- **Duplicate detection** and packet loss simulation
- **Multi-socket support** (up to 15 concurrent sockets)

## Key Components
### Library Functions
- `k_socket()`: Creates a KTP socket
- `k_bind()`: Binds source/destination addresses
- `k_sendto()`: Sends messages reliably
- `k_recvfrom()`: Receives messages in-order
- `k_close()`: Cleans up socket resources

### System Architecture
1. **Shared Memory**: Stores socket state information
2. **Thread R**: Handles incoming messages and ACKs
3. **Thread S**: Manages timeouts and retransmissions
4. **Garbage Collector**: Cleans up after terminated processes

## Building and Running
### Prerequisites
- Linux environment
- GCC compiler
- Standard C libraries

### Build Instructions
```bash
# Build static library
make lib

# Build initialization daemon
make init

# Build sample applications
make users
