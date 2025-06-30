/*
    Assignment 4 Submission
    Kondapalli Mohan Balasubramanyam
    22CS10036
*/

#include "ksocket.h"

/* |-------------------------------------------------------------------| */

// Global Variables to be used in this file
static struct sembuf pop = {0, -1, 0}, vop = {0, 1, 0};

// Macros for semaphore operations
#define sem_wait(s) semop(s, &pop, 1)
#define sem_signal(s) semop(s, &vop, 1)

/* |-------------------------------------------------------------------| */

// Return the successor of the given sequence number
int successor(int val)
{
    val++;
    if (val == MAXSEQNO)
        val = 1;
    return val;
}

// Helper function for comparing two sequence numbers, considering the wraparound
int check_less_than(int a, int b)
{
    if (a == b)
        return 0;
    if (a < b && (b - a) <= MAXMSGS - 1)
        return 1;
    if (a > b && (b + MAXSEQNO - a - 1) <= MAXMSGS - 1)
        return 1;
    if (a < b && (b - a) < 2 * MAXMSGS)
        return 1;
    if (a > b && (b + MAXSEQNO - a - 1) < 2 * MAXMSGS)
        return 1;
    return 0;
}

// Function used for simulating real life network, by dropping messages at receiver end randomly
int drop_message(float p)
{
    double num = (double)rand() / RAND_MAX;

    if (num < p)
        return 1;
    else
        return 0;
}

/* |-------------------------------------------------------------------| */

int k_socket(int domain, int type, int protocol)
{
    // This function only accepts SOCK_KTP as the socket_type
    if (type != SOCK_KTP)
    {
        fprintf(stderr, "### ERROR: k_socket doesn't accept any type other than SOCK_KTP\n");
        exit(3);
    }

    // Fetch the shared memory
    int shmid = shmget(KEY, 0, 0);
    shared_mem *SM = (shared_mem *)shmat(shmid, 0, 0);
    int semid = semget(SEMKEY, 1, 0);

    // Check for free space till shared_mem_index th index in SM
    for (int i = 0; i < N; i++)
    {
        sem_wait(semid);
        if (SM[i].status == 'F')
        {
            // Allocating a new KTP Socket in the Shared Memory
            SM[i].status = 'a';
            SM[i].pid = getpid();
            SM[i].domain = domain;
            SM[i].protocol = protocol;
            SM[i].gen_seqno = 1;
            SM[i].nospace = 0;
            SM[i].last_in_order_seqno = 0;
            SM[i].recv_buf_exp_seq = 1;
            SM[i].src.sin_family = SM[i].dest.sin_family = AF_INET;
            SM[i].src.sin_addr.s_addr = SM[i].dest.sin_addr.s_addr = inet_addr("8.8.8.8");
            SM[i].src.sin_port = SM[i].dest.sin_port = htons(10);
            SM[i].send_buffer.st = SM[i].send_buffer.en = SM[i].receive_buffer.st = SM[i].receive_buffer.en = 0;
            for (int j = 0; j < MAXMSGS; j++)
                SM[i].send_buffer.msgs[j].seq_no = SM[i].receive_buffer.msgs[j].seq_no = -1;
            SM[i].send_buffer.size = SM[i].receive_buffer.size = 0;
            for (int j = 0; j < MAXMSGS; j++)
                SM[i].swnd.seq_nums[j].seq_no = -1;
            for (int j = 1; j <= MAXMSGS; j++)
                SM[i].rwnd.seq_nums[j - 1].seq_no = j;
            SM[i].next_seq_no = MAXMSGS + 1;
            SM[i].swnd.size = SM[i].rwnd.size = MAXMSGS;

            // Detach the shared memory before returning
            sem_signal(semid);
            shmdt(SM);
            return i;
        }
        sem_signal(semid);
    }

    // If there is no free space left in the SM, then return -1 and set errno to ENOSPACE
    errno = ENOSPACE;
    shmdt(SM);
    return -1;
}

int k_bind(int ksockfd, const struct sockaddr_in src_addr, socklen_t src_addrlen, const struct sockaddr_in dest_addr, socklen_t dest_addrlen)
{
    // Fetch the shared memory and the semaphore variable
    int shmid = shmget(KEY, 0, 0);
    shared_mem *SM = (shared_mem *)shmat(shmid, 0, 0);
    int semid = semget(SEMKEY, 1, 0);

    sem_wait(semid);

    // Update Destination IP and Destination Port in SM
    SM[ksockfd].status = 'b';

    SM[ksockfd].src.sin_family = src_addr.sin_family;
    SM[ksockfd].src.sin_addr.s_addr = src_addr.sin_addr.s_addr;
    SM[ksockfd].src.sin_port = src_addr.sin_port;

    SM[ksockfd].dest.sin_family = dest_addr.sin_family;
    SM[ksockfd].dest.sin_addr.s_addr = dest_addr.sin_addr.s_addr;
    SM[ksockfd].dest.sin_port = dest_addr.sin_port;

    sem_signal(semid);

    // Detach the shared memory before returning
    shmdt(SM);
    return 0;
}

ssize_t k_sendto(int ksockfd, char *buf, const struct sockaddr_in dest_addr, socklen_t dest_addrlen)
{
    // Fetch the shared memory and the semaphore variable
    int shmid = shmget(KEY, 0, 0);
    shared_mem *SM = (shared_mem *)shmat(shmid, 0, 0);
    int semid = semget(SEMKEY, 1, 0);

    sem_wait(semid);

    // Check whether the KTP socket with socket file descriptor sockfd is bounded to the socket with dest_addr and dest_port or not
    if (SM[ksockfd].dest.sin_addr.s_addr != dest_addr.sin_addr.s_addr || SM[ksockfd].dest.sin_port != dest_addr.sin_port)
    {
        errno = ENOTBOUND;

        // Detach the shared memory before returning
        sem_signal(semid);
        shmdt(SM);
        return -1;
    }

    // Check whether there is space left in send_buffer or not
    if (SM[ksockfd].send_buffer.size == MAXMSGS)
    {
        errno = ENOSPACE;

        // Detach the shared memory before returning
        sem_signal(semid);
        shmdt(SM);
        return -1;
    }

    // Write the message to the sender side message buffer
    strcpy(SM[ksockfd].send_buffer.msgs[SM[ksockfd].send_buffer.en].msg, buf);
    SM[ksockfd].send_buffer.msgs[SM[ksockfd].send_buffer.en].seq_no = SM[ksockfd].gen_seqno;
    SM[ksockfd].send_buffer.en = (SM[ksockfd].send_buffer.en + 1) % MAXMSGS;
    SM[ksockfd].send_buffer.size++;

#ifdef VERBOSE
    printf("|=====================================================================================|\n");
    printf(" Wrote the data with seq_no|%d| to sender buffer IP|%s| PORT|%d|\n", SM[ksockfd].gen_seqno, inet_ntoa(SM[ksockfd].src.sin_addr), ntohs(SM[ksockfd].src.sin_port));
    printf("|=====================================================================================|\n\n");
#endif

    SM[ksockfd].gen_seqno = successor(SM[ksockfd].gen_seqno);

    // Detach the shared memory before returning
    sem_signal(semid);
    shmdt(SM);
    return MSGSIZE;
}

ssize_t k_recvfrom(int ksockfd, char *buf)
{
    // Fetch the shared memory and the semaphore variable
    int shmid = shmget(KEY, 0, 0);
    shared_mem *SM = (shared_mem *)shmat(shmid, 0, 0);
    int semid = semget(SEMKEY, 1, 0);

    sem_wait(semid);

    // Check if there is any message already received
    if (SM[ksockfd].receive_buffer.size == 0 || SM[ksockfd].receive_buffer.msgs[SM[ksockfd].receive_buffer.st].seq_no != SM[ksockfd].recv_buf_exp_seq)
    {
        errno = ENOMESSAGE;

        // Detach the shared memory before returning
        sem_signal(semid);
        shmdt(SM);
        return -1;
    }

    // Store the first message to return it, and delete that message from receiver side messsage buffer
    strcpy(buf, SM[ksockfd].receive_buffer.msgs[SM[ksockfd].receive_buffer.st].msg);
    strcpy(SM[ksockfd].receive_buffer.msgs[SM[ksockfd].receive_buffer.st].msg, "\0");
    SM[ksockfd].receive_buffer.msgs[SM[ksockfd].receive_buffer.st].seq_no = -1;
    SM[ksockfd].receive_buffer.st = (SM[ksockfd].receive_buffer.st + 1) % MAXMSGS;
    SM[ksockfd].receive_buffer.size--;
    SM[ksockfd].rwnd.size++;

#ifdef VERBOSE
    printf("|=====================================================================================|\n");
    printf(" Read the data with seq_no|%d| from receiver buffer IP|%s| PORT|%d|\n", SM[ksockfd].recv_buf_exp_seq, inet_ntoa(SM[ksockfd].src.sin_addr), ntohs(SM[ksockfd].src.sin_port));
    for (int i = 0; i < MAXMSGS; i++)
    {
        if (SM[ksockfd].receive_buffer.msgs[i].seq_no == SM[ksockfd].recv_buf_exp_seq)
        {
            printf(" Failed to remove Seq_No|%d| after reading\n", SM[ksockfd].recv_buf_exp_seq);
            sleep(2 * T);
        }
    }
    printf("|=====================================================================================|\n\n");
#endif

    SM[ksockfd].recv_buf_exp_seq = successor(SM[ksockfd].recv_buf_exp_seq);

    // Detach the shared memory before returning
    sem_signal(semid);
    shmdt(SM);
    return MSGSIZE;
}

int k_close(int ksockfd)
{
    // Fetch the shared memory and the semaphore variable
    int shmid = shmget(KEY, 0, 0);
    shared_mem *SM = (shared_mem *)shmat(shmid, 0, 0);
    int semid = semget(SEMKEY, 1, 0);

    sem_wait(semid);

    if (SM[ksockfd].send_buffer.size == 0 && SM[ksockfd].status == 'B')
    {
        SM[ksockfd].status = 'f';
        for (int i = 0; i < MAXMSGS; i++)
        {
            strcpy(SM[ksockfd].send_buffer.msgs[i].msg, "");
            strcpy(SM[ksockfd].receive_buffer.msgs[i].msg, "");
            SM[ksockfd].send_buffer.msgs[i].seq_no = SM[ksockfd].receive_buffer.msgs[i].seq_no = -1;
            SM[ksockfd].swnd.seq_nums[i].seq_no = SM[ksockfd].rwnd.seq_nums[i].seq_no = -1;
        }
        SM[ksockfd].swnd.size = SM[ksockfd].rwnd.size = 0;
        SM[ksockfd].send_buffer.st = SM[ksockfd].send_buffer.en = SM[ksockfd].receive_buffer.st = SM[ksockfd].receive_buffer.en = 0;
        SM[ksockfd].send_buffer.size = SM[ksockfd].receive_buffer.size = 0;
    }

    // Detach the shared memory and semaphore variable before returning
    sem_signal(semid);
    shmdt(SM);
    return 0;
}