#include "common.h"

#define BURST_SIZE 1
#define RTT 2 /* RTT is in milli seconds */

void reformatHostName(char *host_name, size_t max_len);

int gethostname(char*, size_t);

/* Data Structures */
struct WindowElement {
    char sendPkt[MAX_BUF_LENGTH];
    struct timeval lastSentTime;
    int isResent;
    int isSpaceUsed;
    int lastSeqNumSentDuringRetransmit;  /* contains the last sequence number sent when the packet was last re-transmitted */
};
typedef struct WindowElement WindowElement;

void myprintf (__const char *format, ...) {

}

void prepareSendPacketHdr(SendPacketHeader* sendPktHdr, int packetType, int seqNum, int dataLength) {
    sendPktHdr->packetType = packetType;
    sendPktHdr->seqNum = seqNum;
    sendPktHdr->length = dataLength;
}

void incrementWindowIdx(int* windowIdx) {
    *windowIdx = (*windowIdx + 1) % (WINDOW_SIZE);
}

int getWindowIdx(int seqNum) {
    return seqNum % WINDOW_SIZE;
}

int isValidCumulativeAck(int cumulativeAck, int startWindowIdx, WindowElement *window) {
    int startWindowSeqNum = getSeqNum(window[startWindowIdx].sendPkt);
    if (modSubtract(cumulativeAck, startWindowSeqNum, SEQUENCE_SIZE) < WINDOW_SIZE ||
            cumulativeAck == (modSubtract(startWindowSeqNum, 1, SEQUENCE_SIZE))) {
        return 1;
    } else {
        return 0;
    }
}

int isCumulativeAckInsideWindow(int cumulativeAck, int startWindowIdx, WindowElement *window) {
    int startWindowSeqNum = getSeqNum(window[startWindowIdx].sendPkt);
    if (modSubtract(cumulativeAck, startWindowSeqNum, SEQUENCE_SIZE) < WINDOW_SIZE) {
        return 1;
    } else {
        return 0;
    }
}

int isCumulativeOneLessThanStartIdx(int cumulativeAck, int startWindowIdx, WindowElement *window) {
    int startWindowSeqNum = getSeqNum(window[startWindowIdx].sendPkt);
    if (cumulativeAck == (modSubtract(startWindowSeqNum, 1, SEQUENCE_SIZE))) {
        return 1;
    } else {
        return 0;
    }
}

int isGreater(int startSeqNum, int firstNum, int secondNum, int mod){
    int first= modSubtract(firstNum,startSeqNum,mod);
    int second= modSubtract(secondNum,startSeqNum,mod);
    return (first>second);
}

void resendNacks(RespPacketHeader *respPktHdr, int *listOfNacks, WindowElement *window,
        int ss, struct sockaddr_in *send_addr, int startWindowIdx, int lastSeqNumSentDuringRetransmit) {
    /* Resend NACKS logic */
    int tempCtr = 0, totalLength;
    struct timeval curTime;
    for (tempCtr = 0; tempCtr < respPktHdr->numOfNacks; tempCtr++) {
        int nackWindowIdx = getWindowIdx(listOfNacks[tempCtr]);
        if (window[nackWindowIdx].isResent == 0) {
            totalLength = ((SendPacketHeader*) window[getWindowIdx(nackWindowIdx)].sendPkt)->length + sizeof (SendPacketHeader);
            sendto_dbg(ss, window[getWindowIdx(nackWindowIdx)].sendPkt, totalLength, 0,
                    (struct sockaddr *) send_addr, sizeof (*send_addr));
            window[nackWindowIdx].isResent = 1;
            window[nackWindowIdx].lastSeqNumSentDuringRetransmit = lastSeqNumSentDuringRetransmit;
            gettimeofday(&(window[nackWindowIdx].lastSentTime), NULL);
            myprintf("Resent the SeqNum %d for the first time: \n", listOfNacks[tempCtr]);
        } else {
            /* Resend only if the respForSeqNum is greater than lastSeqNumSentDuringRetransmit */
            if(isGreater(getSeqNum(window[startWindowIdx].sendPkt),
                             respPktHdr->respForSeqNum, window[nackWindowIdx].lastSeqNumSentDuringRetransmit, SEQUENCE_SIZE)) {
                totalLength = ((SendPacketHeader*) window[getWindowIdx(nackWindowIdx)].sendPkt)->length + sizeof (SendPacketHeader);
                sendto_dbg(ss, window[getWindowIdx(nackWindowIdx)].sendPkt, totalLength, 0,
                        (struct sockaddr *) send_addr, sizeof (*send_addr));
                window[nackWindowIdx].isResent = 1;
                window[nackWindowIdx].lastSeqNumSentDuringRetransmit = lastSeqNumSentDuringRetransmit;
                gettimeofday(&(window[nackWindowIdx].lastSentTime), NULL);
                myprintf("Retransmit the SeqNum %d in the subsequent time: \n", listOfNacks[tempCtr]);
            } else {
                /* Resend the packets after timeout(RTT) */
                gettimeofday(&curTime, NULL);
                if (computeDiff(curTime, window[nackWindowIdx].lastSentTime) >= RTT) {
                    totalLength = ((SendPacketHeader*) window[getWindowIdx(nackWindowIdx)].sendPkt)->length + sizeof (SendPacketHeader);
                    sendto_dbg(ss, window[getWindowIdx(nackWindowIdx)].sendPkt, totalLength, 0,
                            (struct sockaddr *) send_addr, sizeof (*send_addr));
                    gettimeofday(&(window[nackWindowIdx].lastSentTime), NULL);
                    myprintf("Resent the SeqNum %d after timeout: \n", listOfNacks[tempCtr]);
                } else {
                    myprintf("Not-resending the SeqNum %d because no timeout:(%ld) \n", listOfNacks[tempCtr],computeDiff(curTime, window[nackWindowIdx].lastSentTime));
                }
            }
        }
    }
}

void printPacket(char* sendPacket) {
    SendPacketHeader *sendPktHdr = (SendPacketHeader *) sendPacket;
    char *data = sendPacket + sizeof (SendPacketHeader);
    int i = 0;
    myprintf(" %d | %d | %d ", sendPktHdr->packetType, sendPktHdr->seqNum, sendPktHdr->length);
    /*
    mymyprintf("Data: ");
    for(i=0; i< 64; i++) {
        mymyprintf("%d=%d ", i, sendPacket[i]);
    }
     */
    myprintf("\n ");
}

void printRespPacket(char* respPkt) {
    int i = 0;
    RespPacketHeader *respPktHdr = (RespPacketHeader *) respPkt;
    int *listOfNacks = (int*) (respPkt + sizeof (RespPacketHeader));
    myprintf(" %d | %d | %d | %d [", respPktHdr->ackType, respPktHdr->cumulativeAck, respPktHdr->respForSeqNum, respPktHdr->numOfNacks);
    for (i = 0; i < respPktHdr->numOfNacks; i++) {
        myprintf("%d ", listOfNacks[i]);
    }
    myprintf("] \n");
}

int main(int argc, char **argv) {
    struct sockaddr_in name;
    struct sockaddr_in send_addr;
    struct sockaddr_in from_addr;
    socklen_t from_len;
    struct hostent h_ent;
    struct hostent *p_h_ent;
    char *host_name;
    char my_name[NAME_LENGTH] = {'\0'};
    int host_num;
    int from_ip;
    int ss, sr;
    fd_set mask;
    fd_set dummy_mask, temp_mask;
    int bytes;
    int num;
    char mess_buf[MAX_MESS_LEN];
    struct timeval timeout;

    char *destFileName = "/tmp/ar/archlinux-2011.08.19-core-x86_64.iso\0"; /* TODO */
    char *sourceFileLocation = "/tmp/archlinux-2011.08.19-core-x86_64.iso\0"; /* TODO */
    char sendPkt[MAX_BUF_LENGTH];
    SendPacketHeader *sendPktHdr = (SendPacketHeader *) sendPkt;
    WindowElement window[WINDOW_SIZE];
    int startWindowIdx = 0;
    int curSeqNum = -1;
    int windowSlidedBy = WINDOW_SIZE;
    int tempCtr;
    int tempIdx;
    int lastSeqNumSent = -1;
    int maxSendRemaining;
    int totalLength;
    struct timeval beginTime, endTime, statEndTime, statStartTime;
    int isResendNacks = 0;
    int lossrate;
    int countWindows=0;
    int flag=1;

    /*    RespPacketHeader respPktHdr;
        int listOfNacks[WINDOW_SIZE];*/
    char respPkt[MAX_BUF_LENGTH];
    RespPacketHeader *respPktHdr = (RespPacketHeader *) respPkt;
    int *listOfNacks = (int *) (respPkt + sizeof (RespPacketHeader));

    FILE *fr; /* Pointer to source file, which we read */
    int nread;

    if(argc != 4) {
        printf("Usage: ncp <loss_rate> <source_file_name> <dest_file_name>@<comp_name> \n");
        exit(1);
    }
    lossrate = atoi(argv[1]);
    sendto_dbg_init(lossrate);
    sourceFileLocation = argv[2];
    destFileName = argv[3];
    for(tempCtr = 0; argv[3][tempCtr]!='@'; tempCtr++); /* move till @ character */
    argv[3][tempCtr] = '\0'; /* end of destination file name */
    host_name = &(argv[3][tempCtr+1]);

    sr = socket(AF_INET, SOCK_DGRAM, 0); /* socket for receiving (udp) */
    if (sr < 0) {
        perror("Ncp: socket");
        exit(1);
    }

    name.sin_family = AF_INET;
    name.sin_addr.s_addr = INADDR_ANY;
    name.sin_port = htons(PORT);

    if (bind(sr, (struct sockaddr *) & name, sizeof (name)) < 0) {
        perror("Ncp: bind");
        exit(1);
    }

    ss = socket(AF_INET, SOCK_DGRAM, 0); /* socket for sending (udp) */
    if (ss < 0) {
        perror("Ncp: socket");
        exit(1);
    }

    reformatHostName(host_name, NAME_LENGTH);

    p_h_ent = gethostbyname(host_name);
    if (p_h_ent == NULL) {
        myprintf("Ncp: gethostbyname error.\n");
        exit(1);
    }

    memcpy(&h_ent, p_h_ent, sizeof (h_ent)); 
    memcpy(&host_num, h_ent.h_addr_list[0], sizeof (host_num));

    send_addr.sin_family = AF_INET;
    send_addr.sin_addr.s_addr = host_num;
    send_addr.sin_port = htons(PORT);

    /*
    prepareSendPacketHdr(sendPktHdr, INIT_FILE_TRANSFER, 0, strlen(destFileName) + 1 + strlen(my_name));
    memcpy(sendPkt + sizeof(SendPacketHeader), destFileName, strlen(destFileName));
    memcpy(sendPkt + sizeof(SendPacketHeader) strlen(destFileName) + 1, '@' , 1);
    memcpy(sendPkt + sizeof(SendPacketHeader) strlen(destFileName) + 2, my_name, strlen(my_name));
    memcpy(sendPkt + sizeof(SendPacketHeader) strlen(destFileName) + 3, '\0', 1);
    */

    /* Send a Init File Transfer packet */
    prepareSendPacketHdr(sendPktHdr, INIT_FILE_TRANSFER, 0, strlen(destFileName));
    memcpy(sendPkt + sizeof (SendPacketHeader), destFileName, strlen(destFileName));
    printf("%d %x %x \n", strlen(destFileName)
            , sendPkt
            , sendPkt + sizeof (SendPacketHeader) + strlen(destFileName) + 1);
    sendPkt[sizeof(SendPacketHeader) + strlen(destFileName)] = 0;
    printPacket(sendPkt);
    myprintf("Sending file: %s", sendPkt + sizeof (SendPacketHeader));
    sendto_dbg(ss, sendPkt, sendPktHdr->length + sizeof (SendPacketHeader), 0,
            (struct sockaddr *) & send_addr, sizeof (send_addr));

    FD_ZERO(&mask);
    FD_ZERO(&dummy_mask);
    FD_SET(sr, &mask);
    for (;;) {
        temp_mask = mask;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
        if (num > 0) {
            if (FD_ISSET(sr, &temp_mask)) {
                from_len = sizeof (from_addr);
                recvfrom(sr, respPktHdr, MAX_BUF_LENGTH, 0,
                        (struct sockaddr *) & from_addr,
                        &from_len);
                from_ip = from_addr.sin_addr.s_addr;
                if (respPktHdr->ackType == INIT_FILE_TRANSFER_READY) {
                    printf("Received READY from (%d.%d.%d.%d). Starting to transfer file... \n",
                            (htonl(from_ip) & 0xff000000) >> 24,
                            (htonl(from_ip) & 0x00ff0000) >> 16,
                            (htonl(from_ip) & 0x0000ff00) >> 8,
                            (htonl(from_ip) & 0x000000ff));

                    break;
                } else if (respPktHdr->ackType == INIT_FILE_TRANSFER_BUSY) {
                    myprintf("Received BUSY from (%d.%d.%d.%d). \n",
                            (htonl(from_ip) & 0xff000000) >> 24,
                            (htonl(from_ip) & 0x00ff0000) >> 16,
                            (htonl(from_ip) & 0x0000ff00) >> 8,
                            (htonl(from_ip) & 0x000000ff));
                }
            }
        } else {
            sendto_dbg(ss, sendPkt, sendPktHdr->length + sizeof (SendPacketHeader), 0,
                     (struct sockaddr *) & send_addr, sizeof (send_addr)); /* Resend request after timeout */
            printPacket(sendPkt);
        }
    }
    gettimeofday(&(beginTime), NULL);
    /* Start Sending the file. */
    /* Open the source file for reading */
    if ((fr = fopen(sourceFileLocation, "r")) == NULL) {
        perror("fopen error");
        exit(0);
    }
    myprintf("Opened %s for reading...\n", sourceFileLocation);

    maxSendRemaining = WINDOW_SIZE;
    for (;;) {
        /* Send packets in bursts of 10 */
        tempCtr = 0;
        while ((maxSendRemaining > 0) && (tempCtr++ < BURST_SIZE) && (!feof(fr))) {
            /* Read in a chunk of the file */
            incrementSeqNum(&curSeqNum);
            nread = fread(window[getWindowIdx(curSeqNum)].sendPkt + sizeof (SendPacketHeader), 1, MAX_DATA_LENGTH, fr);
            prepareSendPacketHdr((SendPacketHeader*) (window[getWindowIdx(curSeqNum)].sendPkt), FILE_DATA, curSeqNum, nread);
            window[getWindowIdx(curSeqNum)].isSpaceUsed = 1;
            gettimeofday(&(window[getWindowIdx(curSeqNum)].lastSentTime), NULL);
            window[getWindowIdx(curSeqNum)].isResent = 0;
            sendto_dbg(ss, window[getWindowIdx(curSeqNum)].sendPkt, nread + sizeof (SendPacketHeader), 0,
                    (struct sockaddr *) & send_addr, sizeof (send_addr));
            myprintf("Sent packet : ");
            printPacket(window[getWindowIdx(curSeqNum)].sendPkt);
            lastSeqNumSent = curSeqNum;
            maxSendRemaining--;
            if (feof(fr)) {
                printf("EOF reached!\n");
                break;
            }
        }
/*
        if (feof(fr)) {
            break;
        }
*/

        temp_mask = mask;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
        isResendNacks = 0;
        if (num > 0) {
            if (FD_ISSET(sr, &temp_mask)) {
                from_len = sizeof (from_addr);
                recvfrom(sr, respPkt, MAX_BUF_LENGTH, 0,
                        (struct sockaddr *) & from_addr, &from_len);
                myprintf("%d %d Received packet : ", startWindowIdx, maxSendRemaining);
                printRespPacket(respPkt);
                if (respPktHdr->ackType == ACK_DATA_TRANSFER) {
                    /* Check if this is a valid CumulativeAck */
                    if (isCumulativeOneLessThanStartIdx(respPktHdr->cumulativeAck, startWindowIdx, window)) {
                        isResendNacks = 1;
                    }
                    /* If CumulativeAck is same as the seq num corresponding to startWindowIdx, then slide. */
                    if (isCumulativeAckInsideWindow(respPktHdr->cumulativeAck, startWindowIdx, window)) {
                       if (flag==1) {
                            gettimeofday(&statStartTime, NULL);
                            flag=0;
                        }
                        isResendNacks = 1;
                        while (getSeqNum(window[startWindowIdx].sendPkt) != respPktHdr->cumulativeAck) {
                            window[startWindowIdx].isSpaceUsed = 0;
                            window[startWindowIdx].isResent = 0;
                            incrementWindowIdx(&startWindowIdx);
                            maxSendRemaining++;
                            countWindows++;
                            if ((countWindows * MAX_DATA_LENGTH) % (20 * 1024 * 1024) == 0) {
                                flag=1;
                                /* Print statistics */
                                printf("Total Amount of Data Successfully Sent = %d kBytes. \n", countWindows * MAX_DATA_LENGTH);
                                gettimeofday(&statEndTime, NULL);
                                printf("Transfer rate for last 20MBytes = %.3f Mbits/sec. \n", ((double) (20 * 8 * 1000)) / (computeDiff(statEndTime, statStartTime)));
                            }
                        }
                        window[startWindowIdx].isSpaceUsed = 0;
                        window[startWindowIdx].isResent = 0;
                        incrementWindowIdx(&startWindowIdx);
                        maxSendRemaining++;

                        countWindows++;
                        if ((countWindows * MAX_DATA_LENGTH) % (20 * 1024 * 1024) == 0) {
                            flag=1;
                            /* Print statistics */
                            printf("Total Amount of Data Successfully Sent = %d kBytes. \n", countWindows * MAX_DATA_LENGTH);
                            gettimeofday(&statEndTime, NULL);
                            printf("Transfer rate for last 20MBytes = %.3f Mbits/sec. \n", ((double) (20 * 8 * 1000)) / (computeDiff(statEndTime, statStartTime)));
                        }
                        
                    }
                    if (lastSeqNumSent == respPktHdr->cumulativeAck && feof(fr)) {
                        printf("Breaking \n");
                        break;
                    }
                    if (isResendNacks == 1) {
                        /* Resend NACKS */
                        resendNacks(respPktHdr, listOfNacks, window, ss, &send_addr, startWindowIdx, curSeqNum);
                    }
                }
            }
        }
    }
    gettimeofday(&(endTime), NULL);
    printf("Time taken :%ld ms\n", computeDiff(endTime, beginTime));
    fclose(fr);
    /* Send a File Finish packet */
    prepareSendPacketHdr(sendPktHdr, FILE_FINISH, 0, 0);
    sendto_dbg(ss, sendPkt, sendPktHdr->length + sizeof (SendPacketHeader), 0,
            (struct sockaddr *) & send_addr, sizeof (send_addr));
    for (;;) {
        temp_mask = mask;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
        printf("Waiting for ACK_FINISHED\n");
        if (num > 0) {
            if (FD_ISSET(sr, &temp_mask)) {
                from_len = sizeof (from_addr);
                recvfrom(sr, respPktHdr, MAX_BUF_LENGTH, 0,
                        (struct sockaddr *) & from_addr,
                        &from_len);
                
                if (respPktHdr->ackType == ACK_FINISHED) {
                    printf("Received ACK_FINISHED\n");
                    break;
                }
            }
        } else {
            printf("Resending FILE_FINISH\n");
            prepareSendPacketHdr(sendPktHdr, FILE_FINISH, 0, 0);
            sendto_dbg(ss, sendPkt, sendPktHdr->length + sizeof (SendPacketHeader), 0,
                    (struct sockaddr *) & send_addr, sizeof (send_addr));
        }
    }
    return 0;
}

void reformatHostName(char *host_name, size_t max_len) {
    char *c;
    c = strchr(host_name, '\n');
    if (c) *c = '\0';
    c = strchr(host_name, '\r');
    if (c) *c = '\0';

}