/*
    Assignment 4 Submission
    Kondapalli Mohan Balasubramanyam
    22CS10036
*/

#include "ksocket.h"

int main () {
    struct sockaddr_in src_addr, dest_addr;

    int sockfd;
    if ((sockfd = k_socket(AF_INET, SOCK_KTP, 0)) < 0) {
        exit(5);
    }

    src_addr.sin_family = AF_INET;
    src_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    src_addr.sin_port = htons(20000);

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    dest_addr.sin_port = htons(20100);

    k_bind(sockfd, src_addr, sizeof(src_addr), dest_addr, sizeof(dest_addr));

    FILE *fp = fopen("file1.txt", "r");
    if (fp == NULL) {
        fprintf(stderr, "File1.txt not found in local directory\n");
        exit(6);
    }

    while (1) {
        char c, buff[MSGSIZE];
        for (int i = 0; i < MSGSIZE; ++i) buff[i] = '\0';
        int i, done = 0;
        for (i = 0; i < MSGSIZE - 1; ++i) {
            int isEOF = (fscanf(fp, "%c", &c) == EOF);
            if (isEOF) {
                done = 1;
                buff[i] = '$';
                i++;
                break;
            }
            buff[i] = c;
        }
        buff[i] = '\0';
        while (k_sendto(sockfd, buff, dest_addr, sizeof(dest_addr)) == -1) sleep(2*T);
        if (done) break;
    }

    fclose(fp);
    k_close(sockfd);
}
 