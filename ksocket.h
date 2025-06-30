/*
    Assignment 4 Submission
    Kondapalli Mohan Balasubramanyam
    22CS10036
*/

#ifndef __KSOCKET_H
#define __KSOCKET_H

// Libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>



// Macros

// These values can be changed, if this library is open-sourced
#define N 15
#define T 5
#define P 0.10
#define MSGSIZE 512
#define MAXMSGS 10
#define MAXSEQNO 256

// These values should not be changed at all. 
#define KEY 101
#define SEMKEY 102
#define SOCK_KTP 7
#define DATA_MSG 10000
#define QUERY_MSG 20000
#define ACK_MSG 30000
#define ENOSPACE 400
#define ENOTBOUND 401
#define ENOMESSAGE 402



// Global Variables
extern int errno;



// Typedefs

// Node struct for Window Queue
typedef struct __node {
    int seq_no;                     // For swnd, sequence number of packet sent but not yet acknowledged
                                    // For rwnd, sequence number of packet receiver is expecting to receive

    struct timeval send_timestamp;  // Send_Timestamp is set for storing the latest sent time for that corresponding packet
} win_node;


// Queue struct for Window
typedef struct {
    win_node seq_nums[MAXMSGS]; // Window Queue for storing Sequence Numbers

    int size;                   // Swnd Size for storing corresponding rwnd size at receiver end
} window;


// Node struct for Message Buffer Queue
typedef struct __NODE {
    char msg[MSGSIZE+1];    // Msg character array for storing message of 512 bytes

    int seq_no;             // Sequence number for the corresponding message
} buf_node;


// Queue struct for Message Buffer
typedef struct {
    int st, en;                         // Start and End indices for Message Queue

    buf_node msgs[MAXMSGS];             // Message Buffer Queue for storing messages

    int size;                           // Size of Message Buffer
} msg_buffer;


// Shared Memory struct for storing info for a KTP Socket
typedef struct _SHM {
    char status;                            // Store Status of KTP Socket space in SM like 'f', 'F', 'a', 'A', 'b', 'B'
    
    int pid;                                // Store pid of process creating this socket
    
    int domain, sockfd, protocol;           // Domain, Sockfd, Protocol used in creating this socket
    
    int gen_seqno;                          // Sequence number used for a new message added to this socket's sender_buffer
    
    int nospace;                            // nospace flag, which indicates whether receiver buffer is empty or not
    
    int last_in_order_seqno;                // Sequence number of the last in order message received
    
    int next_seq_no;                        // Next Sequence number to be added to rwnd whenever a number is removed from it
    
    int recv_buf_exp_seq;                   // Sequence number of the next message to be read by the user from receiver_buffer
    
    struct sockaddr_in src, dest;           // Sockaddr_in structs for storing src and dest ips, ports
    
    msg_buffer send_buffer, receive_buffer; // Sender Buffer and Receiver Buffer for 
    
    window swnd, rwnd;                      // Swnd and Rwnd for storing required sequence numbers 
} shared_mem;


// Struct for Packet to be sent through UDP, with necessary information
typedef struct {
    int type;               // 10000 for DATA_MSG, 20000 for QUERY_MSG, 30000 for ACK_MSG
    
    int seq_no;             // Sequence number for corresponding packet
    
    int rwnd_size;          // Piggybacked rwnd size of the receiver side message buffer of destination
    
    char msg[MSGSIZE+1];    // Message of exactly 512 bytes, with appended '\0' (NULL) character
} KTP_Packet;





// Helper Function to compare two sequence numbers taking wrap around into consideration
int successor(int val);

int check_less_than(int a, int b);

char* print_err_msg(int _ERR);

int drop_message(float p);



// Helper Functions for using KTP Sockets
int k_socket (int domain, int type, int protocol);

int k_bind (int ksockfd, const struct sockaddr_in src_addr, socklen_t src_addrlen, const struct sockaddr_in dest_addr, socklen_t dest_addrlen);

ssize_t k_sendto (int ksockfd, char* buf, const struct sockaddr_in dest_addr, socklen_t dest_addrlen);

ssize_t k_recvfrom (int ksockfd, char* buf);

int k_close (int ksockfd);

#endif