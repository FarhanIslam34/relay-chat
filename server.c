/*
 * server.c
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define BACKLOG 10
#define BUF_SIZE 4096
#define NAME_SIZE 32
#define MSG_SIZE 4256
#define START_OF_CURRENT_TIME 11
#define END_OF_CURRENT_TIME 18

void *serve(void *conn_fd);
char *get_nick(char *buf);

pthread_mutex_t mutex;

struct connection{
    int fd; 
    struct connection *next;
    struct connection *last;
};

struct server_arguments{
    int fd; //fd to listen from 
    char name[BUF_SIZE];// Name of client 
    struct connection *first_connection; 
};

int main(int argc, char *argv[])
{
    char *listen_port;
    int listen_fd, conn_fd;
    struct addrinfo hints, *res;
    int rc;
    struct sockaddr_in remote_sa;
    uint16_t remote_port;
    socklen_t addrlen;
    char *remote_ip;
    struct server_arguments server_args;

    listen_port = argv[1];

    /* create a socket */
    if ((listen_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1){
        perror("Socket");
        exit(1);
    }

    /* bind it to a port */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if((rc = getaddrinfo(NULL, listen_port, &hints, &res)) != 0) {
        printf("getaddrinfo failed: %s\n", gai_strerror(rc));
        exit(1);
    }

    if (bind(listen_fd, res->ai_addr, res->ai_addrlen) == -1){
        perror("Bind");
        exit(1);
    }


    /* start listening */
    if (listen(listen_fd, BACKLOG) == -1){
        perror("Listen");
        exit(1);
    }
   
    //Init first and last connection as empty
    struct connection empty_connection;
    empty_connection.fd = 0;
    empty_connection.next = NULL;
    empty_connection.last = NULL;

    server_args.first_connection = &empty_connection; 

    /* infinite loop of accepting new connections and handling them */
    while(1) {
        /* accept a new connection (will block until one appears) */
        addrlen = sizeof(remote_sa);
        conn_fd = accept(listen_fd, (struct sockaddr *) &remote_sa, &addrlen);
        
        /* announce our communication partner */
        remote_ip = inet_ntoa(remote_sa.sin_addr);
        remote_port = ntohs(remote_sa.sin_port);
        printf("new connection from %s:%d\n", remote_ip, remote_port);
        
        //update linked list of clients

        pthread_mutex_lock(&mutex);

        if ((server_args.first_connection)->fd == 0){
            (server_args.first_connection)->fd = conn_fd; 
        }else{
           struct connection *next_node = malloc(sizeof(struct connection));
           struct connection *iterator = server_args.first_connection;
           while (iterator->next != NULL){
               iterator = iterator->next;
           }
           iterator->next = next_node;
           next_node->next = NULL;
           next_node->fd = conn_fd;
        } 

        pthread_mutex_unlock(&mutex);
        
        //Set name for new connection as unknown
        sprintf(server_args.name, "User Unknown (%s:%d)", remote_ip, remote_port);

        //Set the fd for the connection
        server_args.fd = conn_fd;
        
        //make thread for handling client
        pthread_t *client = malloc(sizeof(pthread_t));
        pthread_create(client, NULL, serve, &server_args);
    }
}

void *serve(void *input){
    struct server_arguments *server_args = (struct server_arguments *)input;
    int fd = server_args->fd;
    char name[NAME_SIZE];
    char buf[BUF_SIZE];
    char message[MSG_SIZE];
    int bytes_received;
    time_t send_time;
    struct tm *time_struct;
    char current_time[10];
    char *new_name;
    struct connection *connection_iterator;
    struct connection empty_connection;

    //create empty connection struct
    empty_connection.fd = 0;
    empty_connection.next = NULL;
    empty_connection.last = NULL;

    //init client name from struct
    strcpy(name, server_args->name);

    while((bytes_received = recv(fd, buf, BUF_SIZE, 0)) > 0) {
        fflush(stdout);

        //find time of message
        send_time = time(NULL);
        time_struct = localtime(&send_time);
        strftime(current_time, 10, "%T", time_struct);

        if ((new_name = get_nick(buf)) != NULL){
            //compile all components of message
            sprintf(message, "%s:%s is now %s\n", current_time, name, new_name);

            //print name change on server side
            printf("%s:%s is now %s\n", current_time, name, new_name);

            //rename client
            strcpy(name, new_name);

            

        }else{
            //compile all components of message
            sprintf(message, "%s:%s: %s", current_time, name, buf);
        }

        /* send it back to all clients */
        pthread_mutex_lock(&mutex);
        
        connection_iterator = server_args->first_connection;
        while (connection_iterator != NULL){
            if (send(connection_iterator->fd, message, MSG_SIZE, 0) == -1){
                perror("Send");
                exit(1);
            }
            connection_iterator = connection_iterator->next;
        }

        pthread_mutex_unlock(&mutex);

        memset(buf, '\0',BUF_SIZE);
        memset(message, '\0',MSG_SIZE);
    }
    printf("Lost connection to %s \n", name);
    sprintf(message, "%s: Lost connection to %s\n",current_time, name);

    pthread_mutex_lock(&mutex);

    //Remove node from list of connections
    
    //Find current node 
    connection_iterator = server_args->first_connection;
    while (connection_iterator->fd != fd){
        connection_iterator = connection_iterator->next;
    }
    //If current is the first node
    if (connection_iterator->last == NULL){
        //if current is the only node, reset first connection to empty
        if (connection_iterator->next == NULL)
            server_args->first_connection = &empty_connection;
        //make next node the first node
        else
            server_args->first_connection = connection_iterator->next;
    //if current is the last node, make previous node point to NULL    
    }else if (connection_iterator->next == NULL){
        (connection_iterator->last)->next = NULL;
    //Otherwise, make previous point to the next
    }else{
        (connection_iterator->last)->next = connection_iterator->next;
        (connection_iterator->next)->last = connection_iterator->last;
    }

    pthread_mutex_unlock(&mutex);


    connection_iterator = server_args->first_connection;
    while (connection_iterator != NULL){
        if (send(connection_iterator->fd, message, MSG_SIZE, 0) == -1){
            perror("Send");
            exit(1);
        }
        connection_iterator = connection_iterator->next;
    }

    close(fd);
    return NULL;
}

char *get_nick(char *buf){
    char command[6];
    char *name;
    char *newline;

    //separate input into first 6 characters and rest
    strncpy(command, buf, 6);

    /*if the first 6 characters are "/nick ", make the
    first 32 byte of the rest of the input the new name*/
    if (strcmp(command, "/nick ") == 0){
        name = buf+6;
        newline = strchr(name, '\n');
        *newline = '\0';
        *(name+31) = '\0';
        return name;


    }else{
        return NULL;
    }
}



