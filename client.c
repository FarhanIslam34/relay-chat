/*
 * echo-client.c
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

#define BUF_SIZE 4096

void *recieve(void *conn_fd);

int main(int argc, char *argv[])
{
    char *dest_hostname, *dest_port;
    struct addrinfo hints, *res;
    int conn_fd;
    char buf[BUF_SIZE];
    int n;
    int rc;
    pthread_t thread;

    dest_hostname = argv[1];
    dest_port     = argv[2];

    /* create a socket */
    if ((conn_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1){
        perror("Socket");
        exit(1);
    }

    /* client usually doesn't bind, which lets kernel pick a port number */

    /* but we do need to find the IP address of the server */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if((rc = getaddrinfo(dest_hostname, dest_port, &hints, &res)) != 0) {
        printf("getaddrinfo failed: %s\n", gai_strerror(rc));
        exit(1);
    }

    /* connect to the server */
    if(connect(conn_fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        exit(2);
    }

    printf("Connected\n");

    //Make a thread for receiving 
    pthread_create(&thread,NULL,recieve,&conn_fd);

    /* infinite loop of reading from terminal, sending the data*/
     
    while((n = read(0, buf, BUF_SIZE)) > 0) {
        if (send(conn_fd, buf, n, 0) == -1){
            perror("Send");
            exit(1);
        }

    }

    close(conn_fd);
    printf("Farewell!\n");
}

void *recieve(void *conn_fd){
    char buf[BUF_SIZE];
    int recieved_bytes;
    int fd = *(int *)conn_fd;
    while ((recieved_bytes = recv(fd, buf, BUF_SIZE, 0)) > 0){
        write(0, buf, recieved_bytes);
    }
    printf("Exiting\n");
    exit(1);
    return NULL;
    
}
