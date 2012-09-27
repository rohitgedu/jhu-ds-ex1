#include "net_include.h"

#define BUF_SIZE 1024

long int computeDiff(struct timeval tv1, struct timeval tv2) {
    long int milliSecDiff = 1;
    milliSecDiff = (tv1.tv_sec * 1000 + tv1.tv_usec / 1000) - (tv2.tv_sec * 1000 + tv2.tv_usec / 1000);
    return milliSecDiff;
}

int main(int argc, char **argv)
{
    struct sockaddr_in host;
    struct hostent     h_ent, *p_h_ent;

    char               host_name[80];
    char               *c;

    int                s;
    int                ret;
    int                mess_len;
    char               mess_buf[MAX_MESS_LEN];
    char               *neto_mess_ptr = &mess_buf[sizeof (mess_len)];
    FILE                *fr; /* Pointer to source file, which we read */
    int                 nread;


    int totalSent=0;
    int totalSuccessfullySent=0;
    struct timeval beginTime, endTime, progBeginTime;

    if(argc != 3) {
	printf("Usage: t_ncp <source_file_path> <servername>\n");
	exit(1);
    }    

     /* Open the source file for reading */
    if ((fr = fopen(argv[1], "r")) == NULL) {
        perror("fopen");
        exit(0);
    }
    printf("Opened for reading...\n");

    s = socket(AF_INET, SOCK_STREAM, 0); /* Create a socket (TCP) */
    if (s<0) {
        perror("Net_client: socket error");
        exit(1);
    }

    host.sin_family = AF_INET;
    host.sin_port   = htons(PORT);

    c = strchr(argv[2],'\n'); /* remove new line */
    if ( c ) *c = '\0';
    c = strchr(argv[2],'\r'); /* remove carriage return */
    if ( c ) *c = '\0';

    p_h_ent = gethostbyname(argv[2]);
    if ( p_h_ent == NULL ) {
        printf("net_client: gethostbyname error.\n");
        exit(1);
    }

    memcpy( &h_ent, p_h_ent, sizeof(h_ent) );
    memcpy( &host.sin_addr, h_ent.h_addr_list[0],  sizeof(host.sin_addr) );

    ret = connect(s, (struct sockaddr *)&host, sizeof(host) ); /* Connect! */
    if( ret < 0)
    {
        perror( "Net_client: could not connect to server"); 
        exit(1);
    }
    gettimeofday(&(beginTime), NULL);
    gettimeofday(&progBeginTime, NULL);

    for(;;)
    {
         /* Read in a chunk of the file */
        
        nread = fread(neto_mess_ptr, 1, BUF_SIZE, fr);
        mess_len = nread + sizeof(mess_len);
        memcpy( mess_buf, &mess_len, sizeof(mess_len));
        ret = send( s, mess_buf, mess_len, 0);
        totalSent += mess_len;
        totalSuccessfullySent+=mess_len;
        if (totalSent >(20 * 1024 * 1024)) {
            printf("Total Amount of Data Successfully Sent = %d kBytes. \n", totalSuccessfullySent / 1024);
            gettimeofday(&endTime, NULL);
            printf("Transfer rate for last 20MBytes = %.3f Mbits/sec. \n", ((double) (20 * 8 * 1000)) / (computeDiff(endTime, beginTime)));
            totalSent=0;
            gettimeofday(&beginTime, NULL);
                      
        }

        if (nread < BUF_SIZE) {
            gettimeofday(&(endTime), NULL);
            printf("Time taken :%ld ms\n", computeDiff(endTime, progBeginTime));
            /* Did we reach the EOF? */
            if (feof(fr)) {
                printf("Finished writing.\n");
                break;
            } else {
                printf("An error occurred...\n");
                exit(0);
            }
        }
    }
    return 0;
}

