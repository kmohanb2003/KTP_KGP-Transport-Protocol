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
    src_addr.sin_port = htons(20100);

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    dest_addr.sin_port = htons(20000);

    k_bind(sockfd, src_addr, sizeof(src_addr), dest_addr, sizeof(dest_addr));

    FILE *fp = fopen("file2.txt", "w");

    while (1)
    {
        char buff[MSGSIZE];
        for (int i = 0; i < MSGSIZE; ++i) buff[i] = '\0';
        while (k_recvfrom(sockfd, buff) == -1) {
            continue;
        }
        int done = 0;
        for (int i = 0; i < MSGSIZE; ++i) {
            if (buff[i] == '$') {
                done = 1;
                break;
            }
            if (buff[i] == '\0') break;
            fprintf(fp, "%c", buff[i]);
        }
        if (done) break;
    }

    fclose(fp);
    k_close(sockfd);
    return 0;
}