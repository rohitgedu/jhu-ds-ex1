/* 
 * File:   common.h
 * Author: rsetrav1
 *
 * Created on September 23, 2012, 4:11 PM
 */

#ifndef _COMMON_H
#define	_COMMON_H

#include "net_include.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

#define WINDOW_SIZE 200
#define SEQUENCE_SIZE (2*WINDOW_SIZE)

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
    int respForSeqNum;
    int numOfNacks;
};
typedef struct RespPacketHeader RespPacketHeader;

static int first_time = 1;
static int cutoff = 25; /* default is 25% loss */

void sendto_dbg_init(int percent)
{
	/* percent is in integer form (i.e. 1 = 1%, 5 = 5%) */
	cutoff = percent;
	if( cutoff < 0 ) cutoff = 0;
	else if( cutoff > 100 ) cutoff = 100;

	printf("\n++++++++++ cutoff value is %d +++++++++\n", cutoff);
}

int sendto_dbg(int s, const char *buf, int len, int flags,
	       const struct sockaddr *to, int tolen )
{
	int 	ret;
	int	decision;

	if( first_time )
	{
		struct timeval t;
		gettimeofday( &t, NULL );
		srand( t.tv_sec );
		printf("\n+++++++++\n seed is %d\n++++++++\n", (int)t.tv_sec);
		first_time = 0;
	}
        decision = rand() % 100 + 1;
	if( (cutoff > 0) && (decision <= cutoff ) ) {
		return(len);
	}
	ret = sendto( s, buf, len, flags, to, tolen );
	return( ret );
}

int getSeqNum(char* packet) {
    SendPacketHeader *hdr = (SendPacketHeader*) packet;
    return hdr->seqNum;
}

int modSubtract(int firstNum, int secondNum, int mod) { /* This is firstNUm-secondNum */
    return (((firstNum - secondNum) % mod) + mod) % mod;
}

void incrementSeqNum(int* seqNum) {
    *seqNum = (*seqNum + 1) % (SEQUENCE_SIZE);
}

long int computeDiff(struct timeval tv1, struct timeval tv2) {
    long int milliSecDiff = 1;
    milliSecDiff = (tv1.tv_sec * 1000 + tv1.tv_usec / 1000) - (tv2.tv_sec * 1000 + tv2.tv_usec / 1000);
    return milliSecDiff;
}

#endif	/* _COMMON_H */

