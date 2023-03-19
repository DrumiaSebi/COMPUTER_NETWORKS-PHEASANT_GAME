#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

extern int errno;

#define MESSAGE_LENGTH 100
#define PORT 4446
volatile sig_atomic_t exit_game = 0;

int sd;		

void cyan(){
    printf("\033[0;36m");
}

void green(){
    printf("\033[0;34m");
}

void reset_color(){
    printf("\033[0m");
}

void disconnect_client(){
    exit_game = 1;
}

void send_handler(){
    char message[MESSAGE_LENGTH] = {};

    while(1){
        cyan();
        printf(">: ");
        reset_color();
        fgets(message, MESSAGE_LENGTH, stdin);
        size_t ln = strlen(message) - 1;
        if (message[ln] == '\n'){
            message[ln] = '\0';
        }

        if(strcmp(message, "exit") == 0){
            break;
        }
        else{
            send(sd, message, strlen(message), 0);
        }
        bzero(message, MESSAGE_LENGTH);
    }
}

void recv_handler(){
    char message[MESSAGE_LENGTH] = {};
    while(1){
        int receive = recv(sd, message, MESSAGE_LENGTH, 0);

        if(receive<0){
            perror ("[client]Error while receiveng message().\n");
        }
        else if(receive > 0){
            printf("\r%s\n",message);
            cyan();
            if(strcmp(message,"Server: The word you gave is not valid! You just lost, loser!:") == 0){
                exit_game = 1;
                break;
            }else if(strstr(message, "You just won!") != NULL){
                // printf("Sunt aici si am primit mesaju bun");
                // fflush(stdout);
                bzero(message, MESSAGE_LENGTH);
                strcpy(message, "Yay! Get me out");
                send(sd, message, strlen(message), 0);
                exit_game = 1;
                break;
            }
            printf(">: ");
            reset_color();
            fflush(stdout);
        }else if( receive == 0){
            break;
        }
        bzero(message, MESSAGE_LENGTH);
    }
}

int main(){
    char *ip = "127.0.0.1";	
    struct sockaddr_in server;

    signal(SIGINT, disconnect_client);

    //socket settings
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("Eroare la socket().\n");
      return errno;
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(PORT);

    if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
      perror ("[client]Eroare la connect().\n");
      return errno;
    }

    green();
    printf("~~~ Welcome to the game ~~~\n");
    reset_color();

    pthread_t send_thread;

    if(pthread_create(&send_thread, NULL, (void*)send_handler, NULL) != 0){
        perror ("[client]Thread cretion error - send().\n");
        return errno;
    }

    pthread_t recv_thread;

    if(pthread_create(&recv_thread, NULL, (void*)recv_handler, NULL) != 0){
        perror ("[client]Thread cretion error - receive().\n");
        return errno;
    }

    while(1){
        if(exit_game){
            printf("\nYou will now be disconnected, hope you had fun!\n");
            break;
        }
    }

    close(sd);

    exit(0);
}

