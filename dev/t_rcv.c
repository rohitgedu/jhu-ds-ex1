#include "net_include.h"
#include "time.h"
#define BUF_SIZE 1020

long int computeDiff(struct timeval tv1, struct timeval tv2) {
    long int milliSecDiff = 1;
    milliSecDiff = (tv1.tv_sec * 1000 + tv1.tv_usec / 1000) - (tv2.tv_sec * 1000 + tv2.tv_usec / 1000);
    return milliSecDiff;
}

int main(int argc, char **argv) {
    struct sockaddr_in name;
    int s;
    fd_set mask;
    int recv_s[10];
    int valid[10];
    fd_set dummy_mask, temp_mask;
    int i, j, num;
    int mess_len;
    int neto_len;
    char mess_buf[MAX_MESS_LEN];
    long on = 1;
    int totalBytesRead = 0;
    int totalWritten=0;
    int tempTotalWritten = 0;
   
    FILE *fw; /* Pointer to dest file, which we write  */
    int nwritten, bytes;
    struct timeval beginTime, endTime, progBeginTime;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("Net_server: socket");
        exit(1);
    }

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) & on, sizeof (on)) < 0) {
        perror("Net_server: setsockopt error \n");
        exit(1);
    }

    name.sin_family = AF_INET;
    name.sin_addr.s_addr = INADDR_ANY;
    name.sin_port = htons(PORT);

    if (bind(s, (struct sockaddr *) & name, sizeof (name)) < 0) {
        perror("Net_server: bind");
        exit(1);
    }

    if (listen(s, 4) < 0) {
        perror("Net_server: listen");
        exit(1);
    }

    if(argc != 2) {
        printf("Usage: t_rcv <destination_filepath_with_name>\n");
        exit(1);
    }
    /* Open or create the destination file for writing */
    if ((fw = fopen(argv[1], "w")) == NULL) {
        perror("fopen");
        exit(0);
    }

   
    i = 0;
    gettimeofday(&(beginTime), NULL);
    gettimeofday(&(progBeginTime), NULL);
    FD_ZERO(&mask);
    FD_ZERO(&dummy_mask);
    FD_SET(s, &mask);
    for (;;) {
        temp_mask = mask;
        num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);
        if (num > 0) {
            if (FD_ISSET(s, &temp_mask)) {
                recv_s[i] = accept(s, 0, 0);
                FD_SET(recv_s[i], &mask);
                valid[i] = 1;
                i++;
            }
            for (j = 0; j < i; j++) {
                if (valid[j])
                    if (FD_ISSET(recv_s[j], &temp_mask)) {
                        bytes = recv(recv_s[j], &mess_len, sizeof (mess_len), 0);
                        if (bytes > 0) {
                            neto_len = mess_len - sizeof (mess_len);
                            totalBytesRead = recv(recv_s[j], mess_buf, neto_len, 0);
                            while (totalBytesRead < neto_len) {
                                bytes = recv(recv_s[j], mess_buf + totalBytesRead, neto_len - totalBytesRead, 0);
                                totalBytesRead += bytes;
                            }
                            
                            nwritten = fwrite(mess_buf, 1, totalBytesRead, fw);
                            tempTotalWritten+= nwritten;
                            if(tempTotalWritten >= 20*1024*1024){
                                printf("Total Amount of Data Successfully Received = %d kBytes. \n", totalWritten/1024);
                                gettimeofday(&endTime, NULL);
                                printf("Transfer rate for last 20MBytes = %.3f Mbits/sec. \n", ((double) (20 * 8 * 1000)) / (computeDiff(endTime, beginTime)));
	                        gettimeofday(&(beginTime), NULL);
				totalWritten+=tempTotalWritten;
				tempTotalWritten = 0;
                            }

                        } else {
                             gettimeofday(&endTime, NULL);
                             printf("Total time taken: %ld milliseconds. \n", (computeDiff(endTime, progBeginTime)));
                            printf("closing %d \n", j);
                            FD_CLR(recv_s[j], &mask);
                            close(recv_s[j]);
                            fclose(fw);
                            valid[j] = 0;
			    exit(0);
                        }
                    }
            }
        }
    }
    return 0;
}
