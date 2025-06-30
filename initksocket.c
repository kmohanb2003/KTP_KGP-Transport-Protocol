/*
    Assignment 4 Submission
    Kondapalli Mohan Balasubramanyam
    22CS10036
*/

#include "ksocket.h"

// Global Variables
shared_mem *SM;
int shmid, semid;
static struct sembuf pop = {0, -1, 0}, vop = {0, 1, 0};
pthread_t R, S;
int T_CNT, M_CNT;

// Macros for semaphore operations
#define sem_wait(s) semop(s, &pop, 1)
#define sem_signal(s) semop(s, &vop, 1)

/* |-------------------------------------------------------------------| */

// Function to insert a data message (text) to the buffer, w.r.t the sequence number, such that the resulting buffer has sequnce numbers in increasing order (relatively, considering the wrap-around)
void buf_add(msg_buffer *_BUFF, char *msg, int new_seq_no)
{
    if (_BUFF->size == 0)
    {
        _BUFF->en = (_BUFF->en + 1) % MAXMSGS;
        strcpy(_BUFF->msgs[_BUFF->st].msg, msg);
        _BUFF->msgs[_BUFF->st].seq_no = new_seq_no;
        _BUFF->size++;
    }
    else
    {
        for (int i = _BUFF->st; i != _BUFF->en; i = (i + 1) % MAXMSGS)
        {
            if (_BUFF->msgs[i].seq_no == -1)
                continue;
            if (!check_less_than(_BUFF->msgs[i].seq_no, new_seq_no))
            {
                for (int j = _BUFF->en; j != i; j = (j - 1 + MAXMSGS) % MAXMSGS)
                {
                    strcpy(_BUFF->msgs[j].msg, _BUFF->msgs[(j - 1 + MAXMSGS) % MAXMSGS].msg);
                    _BUFF->msgs[j].seq_no = _BUFF->msgs[(j - 1 + MAXMSGS) % MAXMSGS].seq_no;
                }
                _BUFF->en = (_BUFF->en + 1) % MAXMSGS;
                strcpy(_BUFF->msgs[i].msg, msg);
                _BUFF->msgs[i].seq_no = new_seq_no;
                _BUFF->size++;
                return;
            }
        }
        strcpy(_BUFF->msgs[_BUFF->en].msg, msg);
        _BUFF->msgs[_BUFF->en].seq_no = new_seq_no;
        _BUFF->en = (_BUFF->en + 1) % MAXMSGS;
        _BUFF->size++;
    }
}

// Function to remove a data message (text) to the buffer
void buf_remove(msg_buffer *_BUFF, int rem_seq_no)
{
    if (_BUFF->msgs[_BUFF->st].seq_no == rem_seq_no)
    {
        strcpy(_BUFF->msgs[_BUFF->st].msg, "\0");
        _BUFF->msgs[_BUFF->st].seq_no = -1;
        _BUFF->st = (_BUFF->st + 1) % MAXMSGS;
        _BUFF->size--;
    }
    else
    {
        for (int i = (_BUFF->st + 1) % MAXMSGS; i != _BUFF->en; i = (i + 1) % MAXMSGS)
        {
            if (_BUFF->msgs[i].seq_no == rem_seq_no)
            {
                for (int j = i; (j + 1) % MAXMSGS != _BUFF->en; j = (j + 1) % MAXMSGS)
                {
                    strcpy(_BUFF->msgs[j].msg, _BUFF->msgs[(j + 1) % MAXMSGS].msg);
                    _BUFF->msgs[j].seq_no = _BUFF->msgs[(j + 1) % MAXMSGS].seq_no;
                }
                _BUFF->size--;
                _BUFF->en = (_BUFF->en - 1 + MAXMSGS) % MAXMSGS;
                strcpy(_BUFF->msgs[_BUFF->en].msg, "\0");
                _BUFF->msgs[_BUFF->en].seq_no = -1;
                break;
            }
        }
    }
}

// Helper function to add a sequence number to swnd after sending a new data message
//              or to add a new sequence number to rwnd after a data message has been received
void win_enqueue(window *_WIN, int new_seq_num)
{
    for (int i = 0; i < MAXMSGS; i++)
    {
        if (_WIN->seq_nums[i].seq_no == -1)
        {
            _WIN->seq_nums[i].seq_no = new_seq_num;
            gettimeofday(&_WIN->seq_nums[i].send_timestamp, NULL);
            break;
        }
    }
}

// Helper function to remove a sequence number from swnd whenever an acknowledgement is received
//              or to remove a sequence number from rwnd whenever a data message is received
int win_remove(window *_WIN, int rem_seq_no)
{
    for (int i = 0; i < MAXMSGS; i++)
    {
        if (_WIN->seq_nums[i].seq_no == rem_seq_no)
        {
            for (int j = i; j + 1 < MAXMSGS; j++)
                _WIN->seq_nums[j] = _WIN->seq_nums[j + 1];
            _WIN->seq_nums[MAXMSGS - 1].seq_no = -1;
            _WIN->seq_nums[MAXMSGS - 1].send_timestamp.tv_sec = _WIN->seq_nums[MAXMSGS - 1].send_timestamp.tv_usec = 0;
            return rem_seq_no;
        }
    }

    return -1;
}

// Function for removing the acknowledged messages from send buffer and swnd, with confirmed cumulative acknowledged sequence number
void swnd_sendbuf_remove(int sm_index, int seq_no)
{
    int NOT_REM = 1;
    for (int i = 0; i < MAXMSGS; i++)
    {
        if (SM[sm_index].swnd.seq_nums[i].seq_no == -1)
            continue;
        if (!check_less_than(SM[sm_index].swnd.seq_nums[i].seq_no, seq_no))
        {
            for (int j = 0; j < MAXMSGS; j++)
            {
                if (j + i < MAXMSGS)
                    SM[sm_index].swnd.seq_nums[j] = SM[sm_index].swnd.seq_nums[j + i];
                else
                {
                    SM[sm_index].swnd.seq_nums[j].seq_no = -1;
                    SM[sm_index].swnd.seq_nums[j].send_timestamp.tv_sec = SM[sm_index].swnd.seq_nums[j].send_timestamp.tv_usec = 0;
                }
            }
            NOT_REM = 0;
            break;
        }
    }
    if (NOT_REM)
    {
        for (int i = 0; i < MAXMSGS; i++)
        {
            SM[sm_index].swnd.seq_nums[i].seq_no = -1;
            SM[sm_index].swnd.seq_nums[i].send_timestamp.tv_sec = SM[sm_index].swnd.seq_nums[i].send_timestamp.tv_usec = 0;
        }
    }

    NOT_REM = 1;
    if (check_less_than(SM[sm_index].send_buffer.msgs[SM[sm_index].send_buffer.st].seq_no, seq_no))
    {
        for (int i = (SM[sm_index].send_buffer.st + 1) % MAXMSGS; i != SM[sm_index].send_buffer.en; i = (i + 1) % MAXMSGS)
        {
            if (!check_less_than(SM[sm_index].send_buffer.msgs[i].seq_no, seq_no))
            {
                SM[sm_index].send_buffer.size -= (i + MAXMSGS - SM[sm_index].send_buffer.st) % MAXMSGS;
                for (int j = SM[sm_index].send_buffer.st; j != i; j = (j + 1) % MAXMSGS)
                {
                    strcpy(SM[sm_index].send_buffer.msgs[j].msg, "\0");
                    SM[sm_index].send_buffer.msgs[j].seq_no = -1;
                }
                SM[sm_index].send_buffer.st = i;
                NOT_REM = 0;
                break;
            }
        }
        if (NOT_REM)
        {
            for (int i = SM[sm_index].send_buffer.st; i != SM[sm_index].send_buffer.en; i = (i + 1) % MAXMSGS)
            {
                strcpy(SM[sm_index].send_buffer.msgs[i].msg, "\0");
                SM[sm_index].send_buffer.msgs[i].seq_no = -1;
            }
            SM[sm_index].send_buffer.size = 0;
            SM[sm_index].send_buffer.st = SM[sm_index].send_buffer.en;
        }
    }
}

/* |-------------------------------------------------------------------| */

// Function for updating the sequence number of the last in order message received by rwnd
void update_last_in_order_seqno(int sm_index)
{
    for (int i = (SM[sm_index].receive_buffer.st + 1) % MAXMSGS; i != SM[sm_index].receive_buffer.en; i = (i + 1) % MAXMSGS)
    {
        if (!check_less_than(SM[sm_index].last_in_order_seqno, SM[sm_index].receive_buffer.msgs[i].seq_no))
            continue;
        if (SM[sm_index].receive_buffer.msgs[i].seq_no == successor(SM[sm_index].last_in_order_seqno))
        {
            win_remove(&SM[sm_index].rwnd, SM[sm_index].receive_buffer.msgs[i].seq_no);
            SM[sm_index].rwnd.seq_nums[MAXMSGS - 1].seq_no = SM[sm_index].next_seq_no;
            SM[sm_index].next_seq_no = successor(SM[sm_index].next_seq_no);
            SM[sm_index].last_in_order_seqno = SM[sm_index].receive_buffer.msgs[i].seq_no;
        }
        else
            break;
    }
}

/* |-------------------------------------------------------------------| */

// Clean up socket spaces if their corresponding processes have exited
void garbage_collector()
{
    for (int i = 0; i < N; i++)
    {
        sem_wait(semid);
        if (kill(SM[i].pid, 0) != 0 && errno == ESRCH && SM[i].status != 'F' && SM[i].send_buffer.size == 0)
        {
            SM[i].status = 'F';
            if (SM[i].sockfd != -1)
                close(SM[i].sockfd);
            SM[i].domain = SM[i].sockfd = SM[i].protocol = SM[i].pid = -1;
            SM[i].src.sin_family = SM[i].dest.sin_family = AF_INET;
            SM[i].src.sin_addr.s_addr = SM[i].dest.sin_addr.s_addr = inet_addr("8.8.8.8");
            SM[i].src.sin_port = SM[i].dest.sin_port = htons(10);
            SM[i].gen_seqno = SM[i].last_in_order_seqno = SM[i].recv_buf_exp_seq = -1;
            SM[i].nospace = 0;
            for (int j = 0; j < MAXMSGS; j++)
            {
                strcpy(SM[i].send_buffer.msgs[j].msg, "\0");
                strcpy(SM[i].receive_buffer.msgs[j].msg, "\0");
                SM[i].send_buffer.msgs[j].seq_no = SM[i].receive_buffer.msgs[j].seq_no = -1;
                SM[i].swnd.seq_nums[j].seq_no = SM[i].rwnd.seq_nums[j].seq_no = -1;
            }
            SM[i].swnd.size = SM[i].rwnd.size = 0;
            SM[i].send_buffer.st = SM[i].send_buffer.en = SM[i].receive_buffer.st = SM[i].receive_buffer.en = 0;
            SM[i].send_buffer.size = SM[i].receive_buffer.size = 0;

            printf("|==============================================|\n");
            printf(" Cleaned up Socket at Index %d\n", i);
            printf("|==============================================|\n\n");
        }
        sem_signal(semid);
    }
}

/* Functions that can only be used by initksocket process */
void update_sockets()
{
    // Do necessary socket calls, bind calls, close calls if required
    for (int i = 0; i < N; i++)
    {
        sem_wait(semid);
        if (SM[i].status == 'a')
        { // Do a socket call
            // Create a new UDP Socket
            if ((SM[i].sockfd = socket(SM[i].domain, SOCK_DGRAM, SM[i].protocol)) < 0)
            {
#ifdef VERBOSE
                printf("|==============================================|\n");
                printf(" Socket call inside update_sockets call failed\n");
                printf("|==============================================|\n\n");
#endif
            }
            else
            {
                SM[i].status = 'A';
                printf("|==============================================|\n");
                printf(" Socket at index %d allocated\n", i);
                printf("|==============================================|\n\n");
            }
        }
        else if (SM[i].status == 'b')
        { // Do a bind call
            // Check whether Socket was allocated prior to this or not
            // If it was not, then make a socket call before making a bind call
            if (SM[i].sockfd == -1)
            {
                // Create a new UDP Socket
                if ((SM[i].sockfd = socket(SM[i].domain, SOCK_DGRAM, SM[i].protocol)) < 0)
                {
#ifdef VERBOSE
                    printf("|==============================================|\n");
                    printf(" Socket call inside update_sockets call failed\n");
                    printf("|==============================================|\n\n");
#endif
                    sem_signal(semid);
                    continue;
                }
                printf("|==============================================|\n");
                printf(" Socket at index %d allocated\n", i);
                printf("|==============================================|\n\n");
            }

            // Bind the underlying UDP socket
            if (bind(SM[i].sockfd, (struct sockaddr *)&SM[i].src, sizeof(SM[i].src)) < 0)
            {
#ifdef VERBOSE
                printf("|==============================================|\n");
                printf(" Bind call inside update_sockets call failed\n");
                printf("|==============================================|\n\n");
#endif
            }
            else
            {
                SM[i].status = 'B';
                printf("|==============================================|\n");
                printf(" Socket at index %d\n Bounded with IP|%s| PORT|%d|\n", i, inet_ntoa(SM[i].src.sin_addr), ntohs(SM[i].src.sin_port));
                printf("|==============================================|\n\n");
            }
        }
        else if (SM[i].status == 'f')
        { // Do a close call
            // Close the underlying UDP socket descriptor
            if (SM[i].sockfd != -1)
                close(SM[i].sockfd);
            SM[i].sockfd = -1;
            SM[i].status = 'F';
            SM[i].nospace = 0;
            SM[i].pid = -1;
            SM[i].domain = SM[i].protocol = SM[i].pid = SM[i].gen_seqno = SM[i].last_in_order_seqno = SM[i].recv_buf_exp_seq = -1;
            SM[i].src.sin_family = SM[i].dest.sin_family = AF_INET;
            SM[i].src.sin_addr.s_addr = SM[i].dest.sin_addr.s_addr = inet_addr("8.8.8.8");
            SM[i].src.sin_port = SM[i].dest.sin_port = htons(10);
            for (int j = 0; j < MAXMSGS; j++)
            {
                strcpy(SM[i].send_buffer.msgs[j].msg, "\0");
                strcpy(SM[i].receive_buffer.msgs[j].msg, "\0");
                SM[i].send_buffer.msgs[j].seq_no = SM[i].receive_buffer.msgs[j].seq_no = -1;
                SM[i].swnd.seq_nums[j].seq_no = SM[i].rwnd.seq_nums[j].seq_no = -1;
            }
            SM[i].swnd.size = SM[i].rwnd.size = 0;
            SM[i].send_buffer.st = SM[i].send_buffer.en = SM[i].receive_buffer.st = SM[i].receive_buffer.en = 0;
            SM[i].send_buffer.size = SM[i].receive_buffer.size = 0;

            printf("|==============================================|\n");
            printf(" Freed up Socket at index %d\n", i);
            printf("|==============================================|\n\n");
        }
        sem_signal(semid);
    }
}

// Helper function for making sendto by creating a KTP Packet
void send_call(int sm_index, int type, int seq_no, int rwnd_size, char *data)
{
    KTP_Packet packet_msg;
    strcpy(packet_msg.msg, data);
    packet_msg.type = type;
    packet_msg.seq_no = seq_no;
    packet_msg.rwnd_size = rwnd_size;

    sendto(SM[sm_index].sockfd, &packet_msg, sizeof(packet_msg), 0, (struct sockaddr *)&SM[sm_index].dest, sizeof(SM[sm_index].dest));

#ifdef VERBOSE
    printf("|==============================================|\n");
    if (type == DATA_MSG)
    {
        printf(" DATA Message seq_no|%d|", seq_no);
        T_CNT++;
    }
    else if (type == ACK_MSG)
        printf(" ACK Message seq_no|%d|", seq_no);
    else
        printf(" QUERY Message");
    printf("\n Sent to IP|%s| PORT|%d|\n", inet_ntoa(SM[sm_index].dest.sin_addr),
           ntohs(SM[sm_index].dest.sin_port));
    printf("|==============================================|\n\n");
#endif
}

/* |-------------------------------------------------------------------| */

// Start Routine for Thread R, which handles receiving messages for UDP Packets
void *thread_R_main(void *arg)
{
    fd_set rfds;
    while (1)
    {
        // Garbage Collector
        garbage_collector();

        // Update sockets if necessary calls are required
        update_sockets();

        FD_ZERO(&rfds);
        for (int i = 0; i < N; i++)
        {
            sem_wait(semid);
            if (SM[i].status == 'B')
                FD_SET(SM[i].sockfd, &rfds);
            sem_signal(semid);
        }

        struct timeval tv;
        tv.tv_sec = T;
        tv.tv_usec = 0;

        int ret_val = select(FD_SETSIZE, &rfds, NULL, NULL, &tv);
        if (ret_val == -1)
        {
            fprintf(stderr, "SELECT FAILURE, and Thread R exited\n");
            pthread_exit(NULL);
        }
        else if (ret_val == 0)
        { // Timeout
            for (int i = 0; i < N; i++)
            {
                sem_wait(semid);
                if ((SM[i].status == 'B') && SM[i].nospace == 1 && SM[i].rwnd.size != 0)
                {
                    SM[i].nospace = 0;
                    send_call(i, ACK_MSG, SM[i].last_in_order_seqno, SM[i].rwnd.size, "");
                }
                sem_signal(semid);
            }
        }
        else
        { // Received a data message or an ack message or a query message
            for (int i = 0; i < N; i++)
            {
                sem_wait(semid);
                if (SM[i].status == 'B' && FD_ISSET(SM[i].sockfd, &rfds))
                {
                    KTP_Packet data_msg;
                    int val = recvfrom(SM[i].sockfd, &data_msg, sizeof(data_msg), 0, NULL, NULL);
                    if (val < 0)
                    {
                        fprintf(stderr, "Recvfrom in Thread R failed\n");
                        exit(4);
                    }
                    else if (val > 0)
                    {
                        if (drop_message(P))
                        {
#ifdef VERBOSE
                            printf("|==============================================|\n");
                            if (data_msg.type == DATA_MSG)
                                printf(" DATA Message seq_no|%d|", data_msg.seq_no);
                            else if (data_msg.type == ACK_MSG)
                                printf(" ACK Message seq_no|%d| rwnd size|%d|", data_msg.seq_no, data_msg.rwnd_size);
                            else
                                printf(" QUERY Message");
                            printf("\n Dropped at IP|%s| PORT|%d|\n", inet_ntoa(SM[i].src.sin_addr), ntohs(SM[i].src.sin_port));
                            printf("|==============================================|\n\n");
#endif
                            sem_signal(semid);
                            continue;
                        }
#ifdef VERBOSE
                        printf("|==============================================|\n");
                        if (data_msg.type == DATA_MSG)
                            printf(" DATA Message seq_no|%d|", data_msg.seq_no);
                        else if (data_msg.type == ACK_MSG)
                            printf(" ACK Message seq_no|%d| rwnd size|%d|", data_msg.seq_no, data_msg.rwnd_size);
                        else
                            printf(" QUERY Message");
                        printf("\n Received at IP|%s| PORT|%d|\n", inet_ntoa(SM[i].src.sin_addr), ntohs(SM[i].src.sin_port));
                        printf("|==============================================|\n\n");
#endif
                        if (data_msg.type == DATA_MSG)
                        {
                            // Check whether this is a delayed duplicate
                            if (!check_less_than(SM[i].last_in_order_seqno, data_msg.seq_no))
                            {
                                // If yes, then send an ACK with last_in_order msg sequence number,
                                // to let the sender know that all the messages till this seq number
                                // have been received
                                send_call(i, ACK_MSG, SM[i].last_in_order_seqno, SM[i].rwnd.size, "");
                                sem_signal(semid);
                                continue;
                            }

                            // If it is a new data message, not present in the receiver side message buffer
                            // then add it to the receiver side message buffer, and remove it from the rwnd

                            // Check whether this message was an in order message or not
                            if (SM[i].rwnd.size != 0 && SM[i].rwnd.seq_nums[0].seq_no == data_msg.seq_no)
                            { // If yes, then add it to the receiver buffer, and remove it from rwnd
                                buf_add(&SM[i].receive_buffer, data_msg.msg, data_msg.seq_no);
                                SM[i].rwnd.size--;
                                win_remove(&SM[i].rwnd, data_msg.seq_no);
                                SM[i].rwnd.seq_nums[MAXMSGS - 1].seq_no = SM[i].next_seq_no;
                                SM[i].next_seq_no = successor(SM[i].next_seq_no);
                                SM[i].last_in_order_seqno = data_msg.seq_no;
                                update_last_in_order_seqno(i);
                                send_call(i, ACK_MSG, SM[i].last_in_order_seqno, SM[i].rwnd.size, "");
                            }
                            // If it was out of order, then it is just added to the receiver buffer only if was not received earlier
                            else if (SM[i].rwnd.size != 0 && check_less_than(SM[i].rwnd.seq_nums[0].seq_no, data_msg.seq_no) && !check_less_than(SM[i].rwnd.seq_nums[MAXMSGS - 1].seq_no, data_msg.seq_no))
                            {
                                // Check whether this is a duplicate or not
                                int ABS_FLAG = 1;
                                for (int j = SM[i].receive_buffer.st; j != SM[i].receive_buffer.en; j = (j + 1) % MAXMSGS)
                                {
                                    if (SM[i].receive_buffer.msgs[j].seq_no == data_msg.seq_no)
                                    {
                                        ABS_FLAG = 0;
                                        break;
                                    }
                                }

                                // Else add it to the receiver buffer
                                if (ABS_FLAG)
                                {
                                    buf_add(&SM[i].receive_buffer, data_msg.msg, data_msg.seq_no);
                                    SM[i].rwnd.size--;
                                }
                            }

                            if (SM[i].rwnd.size == 0)
                                SM[i].nospace = 1;
                        }
                        else if (data_msg.type == ACK_MSG)
                        {
                            // Check whether this is an ACK_MSG received for a msg within current swnd
                            int PST_FLAG = 0;

                            for (int j = 0; j < MAXMSGS; j++)
                            {
                                if (SM[i].swnd.seq_nums[j].seq_no == data_msg.seq_no)
                                {
                                    buf_remove(&SM[i].send_buffer, data_msg.seq_no);
                                    PST_FLAG = 1;
                                    break;
                                }
                            }
                            if (PST_FLAG)
                            {
                                win_remove(&SM[i].swnd, data_msg.seq_no);
                                swnd_sendbuf_remove(i, data_msg.seq_no);
                                SM[i].swnd.size = data_msg.rwnd_size;
                                sem_signal(semid);
                                continue;
                            }

                            // Check whether this is a cumulative ACK message which requires to
                            // remove old messages from sender_buffer and swnd already acknowledged
                            if (SM[i].swnd.seq_nums[0].seq_no != -1 && check_less_than(SM[i].swnd.seq_nums[0].seq_no, data_msg.seq_no))
                            {
                                swnd_sendbuf_remove(i, data_msg.seq_no);
                                SM[i].swnd.size = data_msg.rwnd_size;
                                sem_signal(semid);
                                continue;
                            }

                            // Check whether this is a duplicate ACK message which requires sender
                            // to update swnd size
                            if (SM[i].swnd.seq_nums[0].seq_no != -1 && check_less_than(data_msg.seq_no, SM[i].swnd.seq_nums[0].seq_no))
                            {
                                SM[i].swnd.size = data_msg.rwnd_size;
                            }
                            else if (SM[i].swnd.seq_nums[0].seq_no == -1 && check_less_than(data_msg.seq_no, SM[i].gen_seqno))
                            {
                                SM[i].swnd.size = data_msg.rwnd_size;
                            }
                        }
                        else if (data_msg.type == QUERY_MSG)
                        {
                            // Check whether rwnd size has changed or not
                            if (SM[i].rwnd.size != 0)
                            {
                                send_call(i, ACK_MSG, SM[i].last_in_order_seqno, SM[i].rwnd.size, "");
                            }
                        }
                    }
                }
                sem_signal(semid);
            }
        }
    }
}

/* |-------------------------------------------------------------------| */

// Start Routine for Thread S, which handles retransmissions and timeouts for UDP Packets
void *thread_S_main(void *arg)
{
    struct timeval cur_time, last_sent[N];
    for (int i = 0; i < N; i++)
        gettimeofday(&last_sent[i], NULL);
    while (1)
    {
        // Sleep for T/2 seconds
        sleep(T / 2);
        if (T % 2 == 1)
            usleep(500000);

        // Garbage Collector
        garbage_collector();

        // Update sockets if necessary calls are required
        update_sockets();

        // Check for Timeout Period
        gettimeofday(&cur_time, NULL);
        double diff_time;
        for (int i = 0; i < N; i++)
        {
            sem_wait(semid);
            if (SM[i].status == 'B')
            {
                diff_time = 0;
                if (SM[i].swnd.seq_nums[0].seq_no != -1)
                    diff_time = (double)(cur_time.tv_sec - SM[i].swnd.seq_nums[0].send_timestamp.tv_sec) + (double)(cur_time.tv_usec - SM[i].swnd.seq_nums[0].send_timestamp.tv_usec) * 1e-6;
                else
                    diff_time = (double)(cur_time.tv_sec - last_sent[i].tv_sec) + (double)(cur_time.tv_usec - last_sent[i].tv_usec) * 1e-6;
                if (SM[i].swnd.size == 0)
                { // Receiver side message buffer is filled, so require for an update on rwnd size
                    if (diff_time >= T)
                        send_call(i, QUERY_MSG, 0, 0, "");
                }
                // Retransmit all messages in swnd, and then
                // check for current swnd size of each KTP Socket, and send all pending messages from
                // the buffer that can be sent
                else
                {
                    int temp_var = SM[i].swnd.size;
                    int ABS_FLAG1 = 1;
                    for (int j = 0; j < MAXMSGS && temp_var > 0; j++)
                    {
                        if (SM[i].send_buffer.msgs[SM[i].send_buffer.st].seq_no == SM[i].swnd.seq_nums[j].seq_no && SM[i].swnd.seq_nums[j].seq_no != -1)
                        {
                            if (diff_time >= T)
                            {
                                send_call(i, DATA_MSG, SM[i].swnd.seq_nums[j].seq_no, 0, SM[i].send_buffer.msgs[SM[i].send_buffer.st].msg);
                                gettimeofday(&SM[i].swnd.seq_nums[j].send_timestamp, NULL);
                                temp_var--;
                                gettimeofday(&last_sent[i], NULL);
                            }
                            ABS_FLAG1 = 0;
                            break;
                        }
                    }
                    if (ABS_FLAG1 && temp_var > 0 && SM[i].send_buffer.size != 0)
                    {
                        send_call(i, DATA_MSG, SM[i].send_buffer.msgs[SM[i].send_buffer.st].seq_no, 0, SM[i].send_buffer.msgs[SM[i].send_buffer.st].msg);
                        win_enqueue(&SM[i].swnd, SM[i].send_buffer.msgs[SM[i].send_buffer.st].seq_no);
                        temp_var--;
                        gettimeofday(&last_sent[i], NULL);
#ifdef VERBOSE
                        M_CNT++;
#endif
                    }
                    for (int j = (SM[i].send_buffer.st + 1) % MAXMSGS; temp_var > 0 && j != SM[i].send_buffer.en; j = (j + 1) % MAXMSGS)
                    {
                        int ABS_FLAG2 = 1;
                        for (int k = 0; k < MAXMSGS; k++)
                        {
                            if (SM[i].swnd.seq_nums[k].seq_no == SM[i].send_buffer.msgs[j].seq_no && SM[i].swnd.seq_nums[k].seq_no != -1)
                            {
                                if (diff_time >= T)
                                {
                                    send_call(i, DATA_MSG, SM[i].swnd.seq_nums[k].seq_no, 0, SM[i].send_buffer.msgs[j].msg);
                                    gettimeofday(&SM[i].swnd.seq_nums[k].send_timestamp, NULL);
                                    temp_var--;
                                    gettimeofday(&last_sent[i], NULL);
                                }
                                ABS_FLAG2 = 0;
                                break;
                            }
                        }
                        if (ABS_FLAG2 && temp_var > 0 && SM[i].send_buffer.size != 0)
                        {
                            send_call(i, DATA_MSG, SM[i].send_buffer.msgs[j].seq_no, 0, SM[i].send_buffer.msgs[j].msg);
                            win_enqueue(&SM[i].swnd, SM[i].send_buffer.msgs[j].seq_no);
                            temp_var--;
                            gettimeofday(&last_sent[i], NULL);
#ifdef VERBOSE
                            M_CNT++;
#endif
                        }
                    }
                }
            }
            sem_signal(semid);
        }
    }
}

/* |-------------------------------------------------------------------| */

int main()
{
    srand((int)time(NULL));

    // Create a shared memory, and attach it with global variable SM,
    shmid = shmget(KEY, N * sizeof(shared_mem), IPC_CREAT | IPC_EXCL | 0777);
    SM = (shared_mem *)shmat(shmid, 0, 0);

    // and the required semaphore variable for ensuring mutual exclusion
    semid = semget(SEMKEY, 1, IPC_CREAT | IPC_EXCL | 0777);
    semctl(semid, 0, SETVAL, 1);

    // Initialize the shared memory created
    for (int i = 0; i < N; i++)
    {
        sem_wait(semid);
        SM[i].status = 'F';
        SM[i].domain = SM[i].sockfd = SM[i].protocol = SM[i].pid = -1;
        SM[i].src.sin_family = SM[i].dest.sin_family = AF_INET;
        SM[i].src.sin_addr.s_addr = SM[i].dest.sin_addr.s_addr = inet_addr("8.8.8.8");
        SM[i].src.sin_port = SM[i].dest.sin_port = htons(10);
        SM[i].gen_seqno = SM[i].last_in_order_seqno = SM[i].recv_buf_exp_seq = -1;
        SM[i].nospace = 0;
        SM[i].send_buffer.st = SM[i].send_buffer.en = SM[i].receive_buffer.st = SM[i].receive_buffer.en = -1;
        for (int j = 0; j < MAXMSGS; j++)
            SM[i].send_buffer.msgs[j].seq_no = SM[i].receive_buffer.msgs[j].seq_no = -1;
        SM[i].send_buffer.size = SM[i].receive_buffer.size = 0;
        for (int j = 0; j < MAXMSGS; j++)
            SM[i].swnd.seq_nums[j].seq_no = SM[i].rwnd.seq_nums[j].seq_no = -1;
        SM[i].swnd.size = SM[i].rwnd.size = 0;
        sem_signal(semid);
    }

    // Initialzing T_CNT and M_CNT
    T_CNT = M_CNT = 0;

    // Creating Threads R and S
    // If thread creation fails, print an error and exit
    if (pthread_create(&R, NULL, &thread_R_main, NULL))
    {
        fprintf(stderr, "### ERROR: Could not create thread R\n");
        exit(1);
    }
    if (pthread_create(&S, NULL, &thread_S_main, NULL))
    {
        fprintf(stderr, "### ERROR: Could not create thread S\n");
        exit(2);
    }

    // Run a while loop and close this initksocket process only when user enters 'Close' or 'Exit'
    printf("InitSocket process is running...\n");
    printf("To terminate:\n");
    printf("Type \"Close\" or \"Exit\"\n\n");
    fd_set rfds;
    FD_ZERO(&rfds);
    while (1)
    {
        FD_SET(STDIN_FILENO, &rfds);
        int ret_val = select(1, &rfds, NULL, NULL, NULL);
        if (ret_val > 0)
        {
            if (FD_ISSET(STDIN_FILENO, &rfds))
            {
                char line[100];
                int nbytes = read(STDIN_FILENO, line, sizeof(line));
                line[nbytes - 1] = '\0';
                if (strcmp(line, "Close") == 0 || strcmp(line, "Exit") == 0)
                {
                    pthread_cancel(R);
                    pthread_cancel(S);
                    sleep(1);
                    for (int i = 0; i < N; i++)
                    {
                        sem_wait(semid);

                        if (SM[i].status != 'F' && SM[i].sockfd != -1)
                        {
                            close(SM[i].sockfd);
                            if (SM[i].pid != -1)
                                kill(SM[i].pid, SIGTERM);
                        }
                        SM[i].domain = SM[i].sockfd = SM[i].protocol = SM[i].pid = -1;
                        SM[i].src.sin_family = SM[i].dest.sin_family = AF_INET;
                        SM[i].src.sin_addr.s_addr = SM[i].dest.sin_addr.s_addr = inet_addr("8.8.8.8");
                        SM[i].src.sin_port = SM[i].dest.sin_port = htons(10);
                        SM[i].gen_seqno = SM[i].last_in_order_seqno = SM[i].recv_buf_exp_seq = -1;
                        SM[i].nospace = 0;
                        for (int j = 0; j < MAXMSGS; j++)
                        {
                            strcpy(SM[i].send_buffer.msgs[j].msg, "");
                            strcpy(SM[i].receive_buffer.msgs[j].msg, "");
                            SM[i].send_buffer.msgs[j].seq_no = SM[i].receive_buffer.msgs[j].seq_no = -1;
                            SM[i].swnd.seq_nums[j].seq_no = SM[i].rwnd.seq_nums[j].seq_no = -1;
                        }
                        SM[i].swnd.size = SM[i].rwnd.size = 0;
                        SM[i].send_buffer.st = SM[i].send_buffer.en = SM[i].receive_buffer.st = SM[i].receive_buffer.en = -1;
                        SM[i].send_buffer.size = SM[i].receive_buffer.size = 0;

                        sem_signal(semid);
                    }

#ifdef VERBOSE
                    printf("\nTotal Number of Transmissions: %d\n", T_CNT);
                    printf("Total Number of Messages Generated from File: %d\n", M_CNT);
#endif

                    shmdt(SM);
                    semctl(semid, 0, IPC_RMID, 0);
                    shmctl(shmid, IPC_RMID, NULL);
                    exit(0);
                }
                printf("\nTo terminate:\n");
                printf("Type \"Close\" or \"Exit\"\n\n");
            }
        }
    }
}
