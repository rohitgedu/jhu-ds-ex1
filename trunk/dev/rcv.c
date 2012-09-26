#include "common.h"
/* #include "sendto_dbg.h" */
/*/ #include "time.h" /*/

#define ADDR_QUEUE_SIZE 20
#define RESPONSE_BURST_SIZE 25
#define RESPONSE_BURST_TIME 5 /* in mill seconds */

int gethostname(char*, size_t);

void PromptForHostName( char *my_name, char *host_name, size_t max_len );

/* Data Structures */
struct BufferElement {
    char senderPkt[MAX_BUF_LENGTH];
    int isSpaceUsed;
};
typedef struct BufferElement BufferElement;

void myprintf (__const char *format, ...) {
   
}

void prepareRespPacketHdr(RespPacketHeader *respPktHdr, int ackType, int cumulativeAck, int numOfNacks, int respForSeqNum) {
    respPktHdr->ackType = ackType;
    respPktHdr->cumulativeAck = cumulativeAck;
    respPktHdr->respForSeqNum = respForSeqNum; 
    respPktHdr->numOfNacks = numOfNacks;
}

int returnGreater(int startSeqNum,int firstNum, int secondNum, int mod){
    int first= modSubtract(firstNum,startSeqNum,mod);
    int second= modSubtract(secondNum,startSeqNum,mod);
    return ((first>second)?firstNum:secondNum);
}

int incrementQueueIdx(int queueIdx, int incrementBy) {
    return (queueIdx + incrementBy) % ADDR_QUEUE_SIZE;
}

int isQueueFull(int *queueSize) {
    return (*queueSize == ADDR_QUEUE_SIZE);
}

int isQueueEmpty(int *queueSize) {
    return (*queueSize == 0);
}

int isExists(struct sockaddr_in *addrQueue, int *addrQueueStart, int *queueSize, 
        struct sockaddr_in addrToAdd) {
    int idx = *addrQueueStart;
    int count = 0;
    while(count++ < *queueSize) {
        if(addrQueue[idx].sin_addr.s_addr == addrToAdd.sin_addr.s_addr) {
            return 1;
        }
        idx = incrementQueueIdx(idx, 1);
    }
    return 0;
}

struct sockaddr_in addAddrIntoQueue(struct sockaddr_in *addrQueue, int *addrQueueStart, int *queueSize, 
        struct sockaddr_in addrToAdd) {
    int idx;
    if(!isExists(addrQueue, addrQueueStart, queueSize, addrToAdd)) {
        idx = incrementQueueIdx(*addrQueueStart, *queueSize);
        addrQueue[idx] = addrToAdd;
        (*queueSize)++;
    }
};

struct sockaddr_in fetchNextAddrFromQueue(struct sockaddr_in *addrQueue, int *addrQueueStart, int *queueSize) {
    struct sockaddr_in retAddr = addrQueue[*addrQueueStart];
    *addrQueueStart = incrementQueueIdx(*addrQueueStart, 1);
    (*queueSize)--;
    return retAddr;
};

void incrementBufferIdx(int* bufferIdx) {
    *bufferIdx = (*bufferIdx + 1) % (WINDOW_SIZE);
}

int getBufferIdx(int seqNum) {
    return seqNum % WINDOW_SIZE;
}

int isValidSeqNum(int seqNum, int startBufferIdx, BufferElement *buffer) {
    int startBufferSeqNum = getSeqNum(buffer[startBufferIdx].senderPkt);
    if (modSubtract(seqNum, startBufferSeqNum, SEQUENCE_SIZE) < WINDOW_SIZE) {
        return 1;
    } else {
        return 0;
    }
}

void printPacket(char* sendPacket) {
   SendPacketHeader *sendPktHdr = (SendPacketHeader *) sendPacket;
   char *data = sendPacket + sizeof(SendPacketHeader);
   int i=0;
   myprintf(" %d | %d | %d ", sendPktHdr->packetType, sendPktHdr->seqNum, sendPktHdr->length);
   myprintf("\n ");
}

void printRespPacket(char* respPkt) {
    int i=0;
    RespPacketHeader *respPktHdr = (RespPacketHeader *)respPkt;
    int *listOfNacks = (int*)(respPkt + sizeof(RespPacketHeader));
    myprintf(" %d | %d | %d | %d [", respPktHdr->ackType, respPktHdr->cumulativeAck, respPktHdr->respForSeqNum, respPktHdr->numOfNacks);
    for(i=0;i<respPktHdr->numOfNacks;i++) {
        myprintf("%d ", listOfNacks[i]);
    }
    myprintf("] \n");
}

int main(int argc, char **argv)
{
    struct sockaddr_in    name;
    struct sockaddr_in    send_addr;
    struct sockaddr_in    from_addr;
    socklen_t             from_len;
    struct hostent        h_ent;
    struct hostent        *p_h_ent;
    char                  host_name[NAME_LENGTH] = "ugrad16\0";
    char                  my_name[NAME_LENGTH] = {'\0'};
    int                   host_num;
    int                   from_ip;
    int                   ss,sr;
    fd_set                mask;
    fd_set                dummy_mask,temp_mask;
    int                   bytes;
    int                   num;
    char                  mess_buf[MAX_MESS_LEN];
    char                  input_buf[80];
    struct timeval        timeout;
    int                   lossrate;
    int                   isFileTransferInProgress = 0;
    int                   isHighestSeqNumSeenDuringWindowSlide = 0;
    int                   highestSeqNumInWindow = 0;
    int                   isHighestSeqNumRecomputeRequired = 1;
    int                   totalElementsInTheWindowToCheck;
    int                   listOfNacksCtr;
    int                   burstCtr = 1;

    struct timeval        burstStartTime;
    struct timeval        curTime;

    struct sockaddr_in    curSenderAddr;
    char senderPkt[MAX_BUF_LENGTH];
    SendPacketHeader *senderPktHdr = (SendPacketHeader *) senderPkt;

    char respPkt[MAX_BUF_LENGTH];
    RespPacketHeader *respPktHdr = (RespPacketHeader *) respPkt;
    int *listOfNacks = (int *)(respPkt + sizeof(RespPacketHeader));
    int cumulativeAck = SEQUENCE_SIZE - 1;
    int tempSeqNum;
    int tempCtr;
    

    struct sockaddr_in addrQueue[ADDR_QUEUE_SIZE];
    int addrQueueStart = 0, queueSize = 0;

    BufferElement buffer[WINDOW_SIZE];
    int startBufferIdx = 0;
    int nwritten;
    /* int lastHighestSeenSeqNum = -1; */

    FILE *fw; /* Pointer to dest file, which we write  */

    sr = socket(AF_INET, SOCK_DGRAM, 0);  /* socket for receiving (udp) */
    if (sr<0) {
        perror("Ucast: socket");
        exit(1);
    }

    name.sin_family = AF_INET; 
    name.sin_addr.s_addr = INADDR_ANY; 
    name.sin_port = htons(PORT);

    if ( bind( sr, (struct sockaddr *)&name, sizeof(name) ) < 0 ) {
        perror("Ucast: bind");
        exit(1);
    }
 
    ss = socket(AF_INET, SOCK_DGRAM, 0); /* socket for sending (udp) */
    if (ss<0) {
        perror("Ucast: socket");
        exit(1);
    }

    if(argc < 2) {
        myprintf("No loss rate specified. Using defaults...\n");
        sendto_dbg_init(0);
    } else {
        lossrate = atoi(argv[1]);
        sendto_dbg_init(lossrate);
        myprintf("Loss rate set to %d\%. \n",lossrate);
    }

    /* PromptForHostName(my_name,host_name,NAME_LENGTH); 
    p_h_ent = gethostbyname(host_name);
    if ( p_h_ent == NULL ) {
        myprintf("Ucast: gethostbyname error.\n");
        exit(1);
    }

    memcpy( &h_ent, p_h_ent, sizeof(h_ent));
    memcpy( &host_num, h_ent.h_addr_list[0], sizeof(host_num) );

    */
    
    send_addr.sin_family = AF_INET;
    /* send_addr.sin_addr.s_addr = 1541463168L; /* TODO: Handle this properly later */
    send_addr.sin_port = htons(PORT);
    

     /* curSenderAddr.sin_addr.s_addr = 0; */
    curSenderAddr = send_addr;
    curSenderAddr.sin_addr.s_addr = 0;
    
    FD_ZERO( &mask );
    FD_ZERO( &dummy_mask );
    FD_SET( sr, &mask );
    for(;;)
    {
        temp_mask = mask;
        timeout.tv_sec = 0;
	timeout.tv_usec = 5000;
        num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
        if (num > 0) {
            if ( FD_ISSET( sr, &temp_mask) ) {
                from_len = sizeof(from_addr);
                bytes = recvfrom( sr, senderPkt, MAX_BUF_LENGTH, 0,
                          (struct sockaddr *)&from_addr, 
                          &from_len );
                myprintf("StrtIdx %d(%d) Recieved packet : ", startBufferIdx, getSeqNum(buffer[startBufferIdx].senderPkt));
                printPacket(senderPkt);
                if(startBufferIdx != getBufferIdx(getSeqNum(buffer[startBufferIdx].senderPkt))) {
                    myprintf("There is something seriously wrong!!!");
                    exit(0);
                }

                if(curSenderAddr.sin_addr.s_addr == 0
                        || from_addr.sin_addr.s_addr == curSenderAddr.sin_addr.s_addr) {
                    curSenderAddr.sin_addr.s_addr = from_addr.sin_addr.s_addr;
                    if(senderPktHdr->packetType == INIT_FILE_TRANSFER) {
                        senderPkt[sizeof(SendPacketHeader) + senderPktHdr->length] = 0;
                        myprintf("Request to create a file: %s \n", senderPkt + sizeof(SendPacketHeader));
                        if((fw = fopen(senderPkt + sizeof(SendPacketHeader), "w")) == NULL){
                            perror("fopen");
                            /* Error occured, so pick the next one from the queue */
                            if(!isQueueEmpty(&queueSize)) {
                                curSenderAddr = fetchNextAddrFromQueue(addrQueue, &addrQueueStart, &queueSize);
                                prepareRespPacketHdr(respPktHdr, INIT_FILE_TRANSFER_READY, 0, 0, 0);
                                sendto_dbg(ss, respPkt, respPktHdr->numOfNacks*sizeof(int) + sizeof (RespPacketHeader), 0,
                                        (struct sockaddr *) & curSenderAddr, sizeof (curSenderAddr));
                            }
                            continue;
                        }
                        /* Send Ready */
                        prepareRespPacketHdr(respPktHdr, INIT_FILE_TRANSFER_READY, 0, 0, 0);
                        sendto_dbg(ss, respPkt, respPktHdr->numOfNacks*sizeof(int) + sizeof (RespPacketHeader), 0,
                                (struct sockaddr *) & curSenderAddr, sizeof (curSenderAddr));
                        isFileTransferInProgress = 1;
                        gettimeofday(&burstStartTime, NULL);

                    } else if(senderPktHdr->packetType == FILE_DATA) {
                        if(isValidSeqNum(senderPktHdr->seqNum, startBufferIdx, buffer)) {
                            /* if this packet is already seen and stored in buffer do not process this packet */
                            if(!(buffer[getBufferIdx(senderPktHdr->seqNum)].isSpaceUsed == 1)) {
                                isHighestSeqNumSeenDuringWindowSlide = 0;
                                isHighestSeqNumRecomputeRequired = 1;
                                memcpy(buffer[getBufferIdx(senderPktHdr->seqNum)].senderPkt, senderPkt, bytes);
                                buffer[getBufferIdx(senderPktHdr->seqNum)].isSpaceUsed = 1;

                                if(senderPktHdr->seqNum == getSeqNum(buffer[startBufferIdx].senderPkt)) {
                                    isHighestSeqNumRecomputeRequired = 0;
                                    while(buffer[startBufferIdx].isSpaceUsed == 1) {
                                        /* Read from buffer and write to the file */
                                        nwritten = fwrite(buffer[startBufferIdx].senderPkt + sizeof(SendPacketHeader), 1,
                                                ((SendPacketHeader*)(buffer[startBufferIdx].senderPkt))->length, fw);
                                        buffer[startBufferIdx].isSpaceUsed = 0;
                                        cumulativeAck = getSeqNum(buffer[startBufferIdx].senderPkt);
                                        if(getSeqNum(buffer[startBufferIdx].senderPkt) == highestSeqNumInWindow) {
                                            isHighestSeqNumSeenDuringWindowSlide = 1;
                                        }
                                        incrementBufferIdx(&startBufferIdx);
                                    }
                                    /* Populate Appropriate Sequence Number so that others are valid */
                                    ((SendPacketHeader*)(buffer[startBufferIdx].senderPkt))->seqNum
                                            = (cumulativeAck + 1) % SEQUENCE_SIZE;
                                    if(isHighestSeqNumSeenDuringWindowSlide == 1) {
                                        highestSeqNumInWindow = getSeqNum(buffer[startBufferIdx].senderPkt);
                                    }
                                }
                                if(isHighestSeqNumRecomputeRequired == 1) {
                                    highestSeqNumInWindow = returnGreater(getSeqNum(buffer[startBufferIdx].senderPkt),
                                                    senderPktHdr->seqNum, highestSeqNumInWindow, SEQUENCE_SIZE);
                                }
                                /* Send Cumulative Ack and Nacks */
                                /* Total Elements in the window to check for unacked packets */
                                totalElementsInTheWindowToCheck = modSubtract(highestSeqNumInWindow,
                                                                        getSeqNum(buffer[startBufferIdx].senderPkt), SEQUENCE_SIZE);

                                for (listOfNacksCtr = 0, tempCtr=0, tempSeqNum = getSeqNum(buffer[startBufferIdx].senderPkt); tempCtr < totalElementsInTheWindowToCheck; tempCtr++, incrementSeqNum(&tempSeqNum)) {
                                    if(buffer[getBufferIdx(tempSeqNum)].isSpaceUsed==0){
                                        listOfNacks[listOfNacksCtr++] = tempSeqNum;
                                    }
                                }
                                prepareRespPacketHdr(respPktHdr,ACK_DATA_TRANSFER,cumulativeAck,listOfNacksCtr, senderPktHdr->seqNum);
                                /* Send response per RESPONSE_BURST_SIZE */
                                gettimeofday(&curTime, NULL);
                                if((burstCtr++ == RESPONSE_BURST_SIZE)
                                        || (computeDiff(curTime, burstStartTime) > RESPONSE_BURST_TIME)) {
                                /* if() { */
                                    sendto_dbg(ss, respPkt, respPktHdr->numOfNacks * sizeof (int) + sizeof (RespPacketHeader), 0,
                                            (struct sockaddr *) & curSenderAddr, sizeof (curSenderAddr));
                                    burstCtr = 1; 
                                    gettimeofday(&burstStartTime, NULL);
                                    myprintf("Sent Response packet : "); printRespPacket(respPkt);
                                } else {
                                    myprintf("%d: Built Response packet : ", computeDiff(curTime, burstStartTime)); printRespPacket(respPkt);
                                }
                            }
                        }
                    } else if(senderPktHdr->packetType == FILE_FINISH) {
                        fclose(fw);
                        prepareRespPacketHdr(respPktHdr, ACK_FINISHED, 0, 0, 0);
                        sendto_dbg(ss, respPkt, respPktHdr->numOfNacks*sizeof(int) + sizeof (RespPacketHeader), 0,
                                (struct sockaddr *) & curSenderAddr, sizeof (curSenderAddr));
                        isFileTransferInProgress = 0;
                        printf("ACK_FINISHED sent to %x ", curSenderAddr.sin_addr.s_addr);
                        /* Pick the next one from the queue */
                        if(!isQueueEmpty(&queueSize)) {
                            curSenderAddr = fetchNextAddrFromQueue(addrQueue, &addrQueueStart, &queueSize);
                            prepareRespPacketHdr(respPktHdr, INIT_FILE_TRANSFER_READY, 0, 0, 0);
                            sendto_dbg(ss, respPkt, respPktHdr->numOfNacks*sizeof(int) + sizeof (RespPacketHeader), 0,
                                    (struct sockaddr *) & curSenderAddr, sizeof (curSenderAddr));
                            isFileTransferInProgress = 1;
                            gettimeofday(&burstStartTime, NULL);
                            burstCtr = 1;
                        } else {
                            curSenderAddr.sin_addr.s_addr = 0;
                        }
                    }
                } else {
                    if(senderPktHdr->packetType == INIT_FILE_TRANSFER) {
                        /* Send Busy */
                        from_addr.sin_family = AF_INET;
                        from_addr.sin_port = htons(PORT);

                        prepareRespPacketHdr(respPktHdr, INIT_FILE_TRANSFER_BUSY, 0, 0, 0);
                        sendto_dbg(ss, respPkt, respPktHdr->numOfNacks*sizeof(int) + sizeof (RespPacketHeader), 0,
                                (struct sockaddr *) & from_addr, sizeof (from_addr));
                        if(!isQueueFull(&queueSize)) {
                            addAddrIntoQueue(addrQueue, &addrQueueStart, &queueSize, from_addr);
                        }
                    } else if(senderPktHdr->packetType == FILE_FINISH) {
                        prepareRespPacketHdr(respPktHdr, ACK_FINISHED, 0, 0, 0);
                        sendto_dbg(ss, respPkt, respPktHdr->numOfNacks*sizeof(int) + sizeof (RespPacketHeader), 0,
                                (struct sockaddr *) & from_addr, sizeof (from_addr));
                        printf("ACK_FINISHED sent to %x ", from_addr.sin_addr.s_addr);
                    }
                    
                }
            }
	} else {
            if(isFileTransferInProgress == 1) {
                sendto_dbg(ss, respPkt, respPktHdr->numOfNacks * sizeof (int) + sizeof (RespPacketHeader), 0,
                        (struct sockaddr *) & curSenderAddr, sizeof (curSenderAddr));
                myprintf("Resending Response packet : "); printRespPacket(respPkt);
            }
            myprintf(".");
            fflush(0);
        }
    }
    
    return 0;

}

void PromptForHostName( char *my_name, char *host_name, size_t max_len ) {

    char *c;

    gethostname(my_name, max_len );
    myprintf("My host name is %s.\n", my_name);

    myprintf( "\nEnter host to send to:\n" );
    if ( fgets(host_name,max_len,stdin) == NULL ) {
        perror("Ucast: read_name");
        exit(1);
    }
    
    c = strchr(host_name,'\n');
    if ( c ) *c = '\0';
    c = strchr(host_name,'\r');
    if ( c ) *c = '\0';

    myprintf( "Sending from %s to %s.\n", my_name, host_name );

}
