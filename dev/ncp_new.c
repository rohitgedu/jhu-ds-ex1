#include "net_include.h"
#include "sendto_dbg.h"
#include "time.h"

#define NAME_LENGTH 80
#define MAX_BUF_LENGTH 1040 /* Actually needed 1036 */
#define MAX_DATA_LENGTH 1024 /* Actually needed 1036 */

/* Sender Packet Types */
#define INIT_FILE_TRANSFER 0
#define FILE_DATA 1
#define FILE_FINISH 2

/* Response Packet Types */
#define INIT_FILE_TRANSFER_READY 0
#define INIT_FILE_TRANSFER_BUSY 1
#define ACK_DATA_TRANSFER 2
#define ACK_FINISHED 3

#define WINDOW_SIZE 100
#define SEQUENCE_SIZE (2*WINDOW_SIZE)


int gethostname(char*, size_t);

void PromptForHostName(char *my_name, char *host_name, size_t max_len);

/* Message Structures */
struct SendPacketHeader {
    int packetType;
    int seqNum;
    int length;
};
typedef struct SendPacketHeader SendPacketHeader;

struct RespPacketHeader {
    int ackType;
    int cumulativeAck;
    int lastSeen;
    int numOfNacks;
};
typedef struct RespPacketHeader RespPacketHeader;

/* Data Structures */
struct WindowElement {
    int isSpaceUsed;
    char sendPkt[MAX_BUF_LENGTH];
    time_t lastSentTime;
    int isResent;
};
typedef struct WindowElement WindowElement;

void prepareSendPacketHdr(SendPacketHeader* sendPktHdr, int packetType, int seqNum, int dataLength) {
    sendPktHdr->packetType = packetType;
    sendPktHdr->seqNum = seqNum;
    sendPktHdr->length = dataLength;
}

void incrementSeqNum(int* seqNum) {
    *seqNum = (*seqNum + 1) % (SEQUENCE_SIZE);
}

void incrementWindowIdx(int* windowIdx) {
    *windowIdx = (*windowIdx + 1) % (WINDOW_SIZE);
}

int getWindowIdx(int seqNum) {
    return seqNum % WINDOW_SIZE;
}

int getSeqNum(char* packet) {
    SendPacketHeader *hdr = (SendPacketHeader*) packet;
    return hdr->seqNum;
}

int modSubtract(int firstNum, int secondNum, int mod) { /* This is firstNUm-secondNum */
    return (((firstNum - secondNum) % mod) + mod) % mod;
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

int main() {
    struct sockaddr_in name;
    struct sockaddr_in send_addr;
    struct sockaddr_in from_addr;
    socklen_t from_len;
    struct hostent h_ent;
    struct hostent *p_h_ent;
    char host_name[NAME_LENGTH] = {'\0'};
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

    char *destFileName = "destFile\0"; /* TODO */
    char *sourceFileLocation = "/tmp/hello.txt\0"; /* TODO */
    char sendPkt[MAX_BUF_LENGTH];
    SendPacketHeader *sendPktHdr = (SendPacketHeader *) sendPkt;
    WindowElement window[WINDOW_SIZE];
    int startWindowIdx = 0;
    int curSeqNum = -1;
    int windowSlidedBy = WINDOW_SIZE;
    int tempCtr;
    int lastSeqNumSent;

    RespPacketHeader respPktHdr;
    int listOfNacks[WINDOW_SIZE];

    FILE *fr; /* Pointer to source file, which we read */
    int nread;
    double RTT = 0.002;

    sr = socket(AF_INET, SOCK_DGRAM, 0); /* socket for receiving (udp) */
    if (sr < 0) {
        perror("Ucast: socket");
        exit(1);
    }

    name.sin_family = AF_INET;
    name.sin_addr.s_addr = INADDR_ANY;
    name.sin_port = htons(PORT);

    if (bind(sr, (struct sockaddr *) & name, sizeof (name)) < 0) {
        perror("Ucast: bind");
        exit(1);
    }

    ss = socket(AF_INET, SOCK_DGRAM, 0); /* socket for sending (udp) */
    if (ss < 0) {
        perror("Ucast: socket");
        exit(1);
    }

    PromptForHostName(my_name, host_name, NAME_LENGTH);

    p_h_ent = gethostbyname(host_name);
    if (p_h_ent == NULL) {
        printf("Ucast: gethostbyname error.\n");
        exit(1);
    }

    memcpy(&h_ent, p_h_ent, sizeof (h_ent)); /* Destination Host Entity */
    memcpy(&host_num, h_ent.h_addr_list[0], sizeof (host_num)); /* Destination Host Address */

    send_addr.sin_family = AF_INET;
    send_addr.sin_addr.s_addr = host_num;
    send_addr.sin_port = htons(PORT);

    /* Send a Init File Transfer packet */
    prepareSendPacketHdr(sendPktHdr, INIT_FILE_TRANSFER, 0, strlen(destFileName));
    memcpy(sendPkt + sizeof (SendPacketHeader), destFileName, strlen(destFileName));

    sendto_dbg_init(0); /* TODO */
    sendto_dbg(ss, sendPkt, sendPktHdr->length + sizeof (SendPacketHeader), 0,
            (struct sockaddr *) & send_addr, sizeof (send_addr));

    FD_ZERO(&mask);
    FD_ZERO(&dummy_mask);
    FD_SET(sr, &mask);
    for (;;) {
        temp_mask = mask;
        num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);
        if (num > 0) {
            if (FD_ISSET(sr, &temp_mask)) {
                from_len = sizeof (from_addr);
                recvfrom(sr, &respPktHdr, sizeof (respPktHdr), 0,
                        (struct sockaddr *) & from_addr,
                        &from_len);
                from_ip = from_addr.sin_addr.s_addr;
                if (respPktHdr.ackType == INIT_FILE_TRANSFER_READY) {
                    printf("Received READY from (%d.%d.%d.%d). Starting to transfer file... \n",
                            (htonl(from_ip) & 0xff000000) >> 24,
                            (htonl(from_ip) & 0x00ff0000) >> 16,
                            (htonl(from_ip) & 0x0000ff00) >> 8,
                            (htonl(from_ip) & 0x000000ff));

                    break;
                } else if (respPktHdr.ackType == INIT_FILE_TRANSFER_BUSY) {
                    printf("Received BUSY from (%d.%d.%d.%d). \n",
                            (htonl(from_ip) & 0xff000000) >> 24,
                            (htonl(from_ip) & 0x00ff0000) >> 16,
                            (htonl(from_ip) & 0x0000ff00) >> 8,
                            (htonl(from_ip) & 0x000000ff));
                }
            }
        }
    }

    /* Start Sending the file. */
    /* Open the source file for reading */
    if ((fr = fopen(sourceFileLocation, "r")) == NULL) {
        perror("fopen error");
        exit(0);
    }
    printf("Opened %s for reading...\n", sourceFileLocation);

    /* We'll read in the file BUF_SIZE bytes at a time, and write it
     * BUF_SIZE bytes at a time.*/
    for (;;) {
        temp_mask = mask;
        /* Check how much space is left in the window */
        tempCtr = 0;
        while (tempCtr++ < windowSlidedBy && !feof(fr)) {
            /* Read in a chunk of the file */
            incrementSeqNum(&curSeqNum);
            nread = fread(window[getWindowIdx(curSeqNum)].sendPkt + sizeof (SendPacketHeader), 1, MAX_DATA_LENGTH, fr);
            prepareSendPacketHdr((SendPacketHeader*)(window[getWindowIdx(curSeqNum)].sendPkt), FILE_DATA, curSeqNum, nread);
            window[getWindowIdx(curSeqNum)].isSpaceUsed = 1;
            window[getWindowIdx(curSeqNum)].lastSentTime = time(NULL);
            window[getWindowIdx(curSeqNum)].isResent = 0;
            sendto_dbg(ss, window[getWindowIdx(curSeqNum)].sendPkt, nread + sizeof (SendPacketHeader), 0,
                    (struct sockaddr *) & send_addr, sizeof (send_addr));
            lastSeqNumSent = curSeqNum;
        }

        /* If there is something to write, write it */
        if (nread > 0) {

        }

        /* Check if anything to receive? */
        windowSlidedBy = 0;
        num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, 0);
        if (num > 0) {
            /* TODO handle resp packets */
            from_len = sizeof (from_addr);
            recvfrom(sr, &respPktHdr, sizeof (respPktHdr), 0,
                    (struct sockaddr *) & from_addr, &from_len);
            recvfrom(sr, listOfNacks, respPktHdr.numOfNacks * sizeof (int), 0,
                    (struct sockaddr *) & from_addr, &from_len);

            if (isValidCumulativeAck(respPktHdr.cumulativeAck, startWindowIdx, window)) {
                windowSlidedBy = (modSubtract(respPktHdr.cumulativeAck, getSeqNum(window[startWindowIdx].sendPkt), SEQUENCE_SIZE) + 1) % WINDOW_SIZE;
            }

            if (lastSeqNumSent == respPktHdr.cumulativeAck && feof(fr)) {
                break;
            }

            /* Slide sender window */
            for (tempCtr = 0; tempCtr < windowSlidedBy; tempCtr++) {
                window[startWindowIdx].isSpaceUsed = 0;
                incrementWindowIdx(&startWindowIdx);
            }

            /* Resend NACKS logic */
            for (tempCtr = 0; tempCtr < respPktHdr.numOfNacks; tempCtr++) {
                int nackWindowIdx = getWindowIdx(listOfNacks[tempCtr]);
                if (window[nackWindowIdx].isResent == 0) {
                    sendto_dbg(ss, window[getWindowIdx(nackWindowIdx)].sendPkt, nread + sizeof (SendPacketHeader), 0,
                            (struct sockaddr *) & send_addr, sizeof (send_addr));
                    window[nackWindowIdx].isResent = 1;
                } else {
                    /* Resend the packets after timeout(RTT) */
                    if (difftime(time(NULL), window[nackWindowIdx].lastSentTime) > RTT) {
                        sendto_dbg(ss, window[getWindowIdx(nackWindowIdx)].sendPkt, nread + sizeof (SendPacketHeader), 0,
                                (struct sockaddr *) & send_addr, sizeof (send_addr));
                        window[nackWindowIdx].lastSentTime = time(NULL);
                    }
                }
            }
        }
    }

    /* Send a File Finish packet */
    prepareSendPacketHdr(sendPktHdr, FILE_FINISH, 0, 0);
    sendto_dbg(ss, sendPkt, sendPktHdr->length + sizeof (SendPacketHeader), 0,
            (struct sockaddr *) & send_addr, sizeof (send_addr));
    for (;;) {
        temp_mask = mask;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
        if (num > 0) {
            if (FD_ISSET(sr, &temp_mask)) {
                from_len = sizeof (from_addr);
                recvfrom(sr, &respPktHdr, sizeof (respPktHdr), 0,
                        (struct sockaddr *) & from_addr,
                        &from_len);
                if (respPktHdr.ackType == ACK_FINISHED) {
                    break;
                }
            }
        } else {
            prepareSendPacketHdr(sendPktHdr, FILE_FINISH, 0, 0);
            sendto_dbg(ss, sendPkt, sendPktHdr->length + sizeof (SendPacketHeader), 0,
                    (struct sockaddr *) & send_addr, sizeof (send_addr));
        }
    }
    return 0;
}

void PromptForHostName(char *my_name, char *host_name, size_t max_len) {

    char *c;

    gethostname(my_name, max_len);
    printf("My host name is %s.\n", my_name);

    printf("\nEnter host to send to:\n");
    if (fgets(host_name, max_len, stdin) == NULL) {
        perror("Ucast: read_name");
        exit(1);
    }

    c = strchr(host_name, '\n');
    if (c) *c = '\0';
    c = strchr(host_name, '\r');
    if (c) *c = '\0';

    printf("Sending from %s to %s.\n", my_name, host_name);

}
