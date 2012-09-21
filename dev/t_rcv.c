#include "net_include.h"

int main()
{
    struct sockaddr_in name;
    int                s;
    fd_set             mask;
    int                recv_s[10];
    int                valid[10];  
    fd_set             dummy_mask,temp_mask;
    int                i,j,num;
    int                mess_len;
    int                neto_len;
    char               mess_buf[MAX_MESS_LEN];
    long               on=1;

    FILE               *fw; /* Pointer to dest file, which we write  */
    char               *dest_file_name = "/home/rsetrav1/destfile\0";
    int                nwritten;
    static unsigned long ct = 0;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s<0) {
        perror("Net_server: socket");
        exit(1);
    }

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
    {
        perror("Net_server: setsockopt error \n");
        exit(1);
    }

    name.sin_family = AF_INET;
    name.sin_addr.s_addr = INADDR_ANY;
    name.sin_port = htons(PORT);

    if ( bind( s, (struct sockaddr *)&name, sizeof(name) ) < 0 ) {
        perror("Net_server: bind");
        exit(1);
    }
 
    if (listen(s, 4) < 0) {
        perror("Net_server: listen");
        exit(1);
    }


    /* Open or create the destination file for writing */
    if((fw = fopen(dest_file_name, "w")) == NULL) {
        perror("fopen");
        exit(0);
    }

    i = 0;
    FD_ZERO(&mask);
    FD_ZERO(&dummy_mask);
    FD_SET(s,&mask);
    for(;;)
    {
        temp_mask = mask;
        num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);
        if (num > 0) {
            if ( FD_ISSET(s,&temp_mask) ) {
                recv_s[i] = accept(s, 0, 0) ;
                FD_SET(recv_s[i], &mask);
                valid[i] = 1;
                i++;
            }
            for(j=0; j<i ; j++)
            {   if (valid[j])    
                if ( FD_ISSET(recv_s[j],&temp_mask) ) {
                    if( recv(recv_s[j],&mess_len,sizeof(mess_len),0) > 0) {
                        neto_len = mess_len - sizeof(mess_len);
                        recv(recv_s[j], mess_buf, neto_len, 0 );

                        /* printf("recieved a message of length :%d \n", neto_len); 
                        fprintf(stderr, "data: 0x%x, len: %d, fp: 0x%x, ct: %d\n",
                            mess_buf, neto_len, fw, ct++); */
                        nwritten = fwrite(mess_buf, 1, neto_len, fw);
                        /*
                        printf("successfully wrote message of length :%d \n", nwritten);
                        //printf("socket is %d ",j);
                        //printf("len is :%d  message is : %s \n ",
                               mess_len,mess_buf); 
                        printf("---------------- \n");
                         */
                    }
                    else
                    {
                        printf("closing %d \n",j);
                        FD_CLR(recv_s[j], &mask);
                        close(recv_s[j]);
                        valid[j] = 0;
                        fclose(fw);
                        printf("File written to :%s \n", dest_file_name);
                    }
                }
            }
        }
    }

    return 0;

}

