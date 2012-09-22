#include "net_include.h"


#define BUF_SIZE 1048
#define DATA_SIZE 1024

int main(int argc, char **argv)



{
    struct sockaddr_in host;
    struct hostent     h_ent, *p_h_ent;

    char               host_name[80];
    char               *c;

    int                s;
    int                ret;
    int                mess_len;
    char               mess_buf[BUF_SIZE];
    char               *neto_mess_ptr = &mess_buf[sizeof(mess_len)]; 
    FILE               *fr; /* Pointer to source file, which we read */
    int                nread;
    int count = 0;

    s = socket(AF_INET, SOCK_STREAM, 0); /* Create a socket (TCP) */
    if (s<0) {
        perror("Net_client: socket error");
        exit(1);
    }

    host.sin_family = AF_INET;
    host.sin_port   = htons(PORT);

    printf("Enter the server name:\n");
    if ( fgets(host_name,80,stdin) == NULL ) {
        perror("net_client: Error reading server name.\n");
        exit(1);
    }
    c = strchr(host_name,'\n'); /* remove new line */
    if ( c ) *c = '\0';
    c = strchr(host_name,'\r'); /* remove carriage return */
    if ( c ) *c = '\0';
    printf("Your server is %s\n",host_name);

    p_h_ent = gethostbyname(host_name);
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

    /* Open the source file for reading */
    if((fr = fopen(argv[1], "r")) == NULL) {
        perror("fopen");
        exit(0);
    }
    printf("Opened %s for reading...\n", argv[1]);
    
    while(!feof(fr))
    {
         /* Read in a chunk of the file */
        nread = fread(neto_mess_ptr, 1, DATA_SIZE, fr);
        mess_len = nread + sizeof(mess_len);
        memcpy( mess_buf, &nread, sizeof(mess_len) );
	printf("Message lenght(%d) %d \n", count++, mess_len);
        ret = send( s, mess_buf, mess_len, 0);
	printf("Message (%d)(%d)(%d)(%d) (%x)(%x)(%x)(%x) \n", mess_buf[0],mess_buf[1],mess_buf[2], mess_buf[3],  mess_buf[4],mess_buf[5],mess_buf[6], mess_buf[7]);
        if(ret != mess_len) 
        {
            perror( "Net_client: error in writing");
            exit(1);
        } 
    }

    return 0;

}
