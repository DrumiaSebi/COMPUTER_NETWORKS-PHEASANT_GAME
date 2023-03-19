#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
// #include <cstdlib>

#define PORT 4446
#define NAME_LEN 10
#define ARRAY_SIZE 100
#define MESSAGE_LENGTH 100
#define DICT "dictionar.txt"

int players_per_session = 0;
static _Atomic unsigned int cli_index = 0;
char last_two_letters[25][2];

extern int errno;

typedef struct{
    int id;
    int sockfd;
    bool sesion_started;
    bool next;
    bool lost;
    char name[NAME_LEN];
} client_struct;

client_struct *clients[ARRAY_SIZE];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void list_add(client_struct *cl){
    pthread_mutex_lock(&clients_mutex);
    clients[cli_index] = cl;
    pthread_mutex_unlock(&clients_mutex);
}

void list_remove(int id){
    pthread_mutex_lock(&clients_mutex);
    if(!clients[id]->lost){
        if(clients[id]->id == id){
            clients[id]->lost = true;
        }
        else{
            perror ("[server]Removing player error[index not matching id].\n");
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_message(char *message, int id, int starting){
    pthread_mutex_lock(&clients_mutex);

    int session_number = id / players_per_session;
    int first_player = session_number * players_per_session;
    int last_player = first_player + players_per_session;
    for(int i=first_player; i<last_player; i++){
        if(clients[i] && !clients[i]->lost){
            if(clients[i]->id != id && !starting){
                if(write(clients[i]->sockfd, message, strlen(message)) < 0){
                    perror ("[server]Error: writing to client descriptor - 1.\n");
                    break;
                }
            }else if(starting){
                if(write(clients[i]->sockfd, message, strlen(message)) < 0){
                    perror ("[server]Error: writing to client descriptor - 2.\n");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

int get_next_id(int id, int session){
    // int next_id = (id + 1) % players_per_session + players_per_session * session;
    int next_id = id;
    bool found_player = false;
    while(!found_player){
        next_id += 1;
        if( (next_id % players_per_session) == 0){
            next_id = players_per_session * session;
        }
        if(!clients[next_id]->lost){
            found_player = true;
            printf("Found next player: %s from session %d\n", clients[next_id]->name, session);
            fflush( stdout );
        }
    }
    return next_id;

}


int get_winner(int id, int session){
    int first_next_id = get_next_id(id, session);
    int second_next_id = get_next_id(first_next_id, session);
    if(first_next_id == second_next_id){
        return first_next_id;
    }
    return -1;
}


void begin_session(int id, int first_player, int last_player){
    bool starter = false;
    char message[MESSAGE_LENGTH];
    for(int i=first_player; i<last_player; i++){
        if(clients[i]){
            clients[i]->sesion_started = true;
            if(!starter){
                clients[i]->next = true;
                strcpy(message, "Please enter a letter you would like your first word to begin with:");
                if(write(clients[i]->sockfd,  message, strlen(message)) < 0){
                    perror ("[server]Error: writing to client descriptor -4.\n");
                    break;
                }
                starter = true;
            }
        }
    }
}

bool is_valid(char to_check[99]){
    FILE * fp;

    fp = fopen(DICT,"r");
    if (!fp) {
        printf("Failed to open specified dictionary %s\n", DICT);
        return -1;
    }

    char word[99];
    while (fscanf(fp, "%98s", word) == 1) {
         if(strcmp(to_check,word) == 0){
            fclose(fp);
            return true;
        }
    }
    return false;
    fclose(fp);
}

void start_game(int session, int id, char * input){
    int receive;
    char message[MESSAGE_LENGTH];
    char rcvmessage[MESSAGE_LENGTH];
    char word[25];
    bzero(message, MESSAGE_LENGTH);
    bzero(rcvmessage, MESSAGE_LENGTH);
    bzero(word, 25);

    //getting the first letter of the word
    if(strlen(input) != 1 || !isalpha(input[0])){
            strcpy(message, "Server: You can only give a single letter as input!");
            if(write(clients[id]->sockfd,  message, strlen(message)) < 0){
            perror ("[server]Error: writing to client descriptor -4.\n");
            }
            bzero(message, MESSAGE_LENGTH);
            while(1){
                receive = recv(clients[id]->sockfd, rcvmessage, MESSAGE_LENGTH, 0);
                if(receive <= 0){
                    perror ("[server]Error: receiving first letter from first player.\n");
                    break;
                }
                if(strlen(rcvmessage) != 1 || !isalpha(rcvmessage[0])){
                    strcpy(message, "Server: You can only give a single letter as input!");
                    if(write(clients[id]->sockfd,  message, strlen(message)) < 0){
                    perror ("[server]Error: writing to client descriptor -4.\n");
                    break;
                    }
                    bzero(message, MESSAGE_LENGTH);
                    bzero(rcvmessage, MESSAGE_LENGTH);
                }else{
                    sprintf(message,"%s has chosen the first letter -> %c.", clients[id]->name, rcvmessage[0]);
                    send_message(message, clients[id]->id, 0);
                    last_two_letters[session][0] = rcvmessage[0];
                    break;
                }
            }
    }
    else{
        sprintf(message,"%s has chosen the first letter -> %c.", clients[id]->name, input[0]);
        send_message(message, clients[id]->id, 0);
        last_two_letters[session][0] = input[0];
    }
    
    //getting first word and the game started
    bzero(message, MESSAGE_LENGTH);
    bzero(rcvmessage, MESSAGE_LENGTH);
    sprintf(message, "Server: Great, now type a word starting with letter \e[1m%c\e[m and get the game going!", last_two_letters[session][0]);
    if(write(clients[id]->sockfd,  message, strlen(message)) < 0){
    perror ("[server]Error: writing to client descriptor -4.\n");
    }
    while (1)
    {
        receive = recv(clients[id]->sockfd, word, 25, 0);
        if(receive <= 0){
                    perror ("[server]Error: receiving first letter from first player.\n");
                    break;
        }
        if(word[0] != last_two_letters[session][0]){
            bzero(message, MESSAGE_LENGTH);
            sprintf(message, "Server: The word you gave doesnt begin with the letter \e[1m%c\e[m !", last_two_letters[session][0]);
            if(write(clients[id]->sockfd,  message, strlen(message)) < 0){
            perror ("[server]Error: writing to client descriptor -4.\n");
            }
        }
        else if(!is_valid(word)){
            bzero(message, MESSAGE_LENGTH);
            strcpy(message, "Server: The word you gave is not valid! Give another one:");
            if(write(clients[id]->sockfd,  message, strlen(message)) < 0){
            perror ("[server]Error: writing to client descriptor -4.\n");
            }
        }
        else{
            // last_two_letters[session][1] = word[1];
            break;
        }
        bzero(word, 25);  
    }
    bzero(message, MESSAGE_LENGTH);
    if(write(clients[id]->sockfd,  "Great! now we wait...", 22) < 0){
        perror ("[server]Error: writing to client descriptor -4.\n");
    }
    sprintf(message, "%s has chosen the word to get the game started: \e[1m%s\e[m !", clients[id]->name, word);
    send_message(message,id,0);
    bzero(message, MESSAGE_LENGTH);
    int last_index = strlen(word) - 1;
    last_two_letters[session][0] = word[last_index - 1];
    last_two_letters[session][1] = word[last_index];
    sprintf(message, "Type a word starting with the last two letters of the previous word: \e[1m%c%c\e[m", last_two_letters[session][0], last_two_letters[session][1]);
    int next_id = (id + 1) % players_per_session + players_per_session * session;
    if(write(clients[next_id]->sockfd,  message, strlen(message)) < 0){
        perror ("[server]Error: writing to client descriptor -4.\n");
    }
    clients[id]->next = false;
    clients[next_id]->next = true;
}

void check_unexpected_exit(int id, int session){
    char message[MESSAGE_LENGTH];
    bzero(message, MESSAGE_LENGTH);
    int winner;
    clients[id]->lost = true;
    winner = get_winner(id, session);
    if(winner > -1){
        strcpy(message, "Server: Congratulations! You just won!");
        if(write(clients[winner]->sockfd,  message, strlen(message)) < 0){
            perror ("[server]Error: writing to client descriptor -4.\n");       
        }
    }else if(clients[id]->next == true){
        sprintf(message, "Type a word starting with the last two letters of the previous word: \e[1m%c%c\e[m", last_two_letters[session][0], last_two_letters[session][1]);
        int next_id = get_next_id(id, session);
        if(write(clients[next_id]->sockfd,  message, strlen(message)) < 0){
            perror ("[server]Error: writing to client descriptor -4.\n");       
        }
        clients[id]->next = false;
        clients[next_id]->next = true;
    }
}

bool respects_rules(char *to_check, int id, int session){
    char message[MESSAGE_LENGTH];
    bzero(message, MESSAGE_LENGTH);
    if(to_check[0] != last_two_letters[session][0] || to_check[1] != last_two_letters[session][1]){
        strcpy(message, "Server: First two letters of the given word dont match the last two of the previous one!");
        if(write(clients[id]->sockfd,  message, strlen(message)) < 0){
        perror ("[server]Error: writing to client descriptor -4.\n");
        }
        return false;
    }
    else if(!is_valid(to_check)){
        strcpy(message, "Server: The word you gave is not valid! You just lost, loser!:");
        if(write(clients[id]->sockfd,  message, strlen(message)) < 0){
            perror ("[server]Error: writing to client descriptor -4.\n");
        }
        return false;
    }
    return true;
}

//alerts the next player that it's his turn
void alert_next_player(int id, char* previous_word, bool kick_player, int session){
    char message[MESSAGE_LENGTH];
    bzero(message, MESSAGE_LENGTH);
    if(kick_player){
        sprintf(message, "%s has been kicked out because he gave an invalid word!", clients[id]->name);
        printf("%s from session %d lost!\n", clients[id]->name, session);
        // fflush( stdout );

    }
    else{
        sprintf(message, "%s has chosen the word: \e[1m%s\e[m !", clients[id]->name, previous_word);
    }
    send_message(message,id,0);
    bzero(message, MESSAGE_LENGTH);
    if(!kick_player){
        int last_index = strlen(previous_word) - 1;
        last_two_letters[session][0] = previous_word[last_index - 1];
        last_two_letters[session][1] = previous_word[last_index];
    }
    else{
        clients[id]->lost = true;
        int winner = get_winner(id, session);
        // printf("Verificarea merge si am id-ul: %d", winner);
        // fflush(stdout);
        if(winner > -1){
            strcpy(message, "Server: Congratulations! You just won!");
            if(write(clients[winner]->sockfd,  message, strlen(message)) < 0){
                perror ("[server]Error: writing to client descriptor -4.\n");       
            }
            return;
        }
    }
    sprintf(message, "Type a word starting with the last two letters of the previous word: \e[1m%c%c\e[m", last_two_letters[session][0], last_two_letters[session][1]);

    // int next_id = (id + 1) % players_per_session + players_per_session * session;
    int next_id = get_next_id(id, session);
    if(next_id != id){
        if(write(clients[next_id]->sockfd,  message, strlen(message)) < 0){
            perror ("[server]Error: writing to client descriptor -4.\n");       
        }
        clients[id]->next = false;
        clients[next_id]->next = true;
    }else{
        bzero(message, MESSAGE_LENGTH);
        strcpy(message, "Server: Congratulations! You just won!");
        if(write(clients[next_id]->sockfd,  message, strlen(message)) < 0){
            perror ("[server]Error: writing to client descriptor -4.\n");       
        }
        clients[id]->next = false;
    }
}

bool play_game(int session, int id, char * input){
    int receive;
    char message[MESSAGE_LENGTH];
    // char rcvmessage[MESSAGE_LENGTH];
    char word[25];
    bzero(message, MESSAGE_LENGTH);
    // bzero(rcvmessage, MESSAGE_LENGTH);
    bzero(word, 25);
    if(!is_valid(input)){
        strcpy(message, "Server: The word you gave is not valid! You just lost, loser!:");
        if(write(clients[id]->sockfd,  message, strlen(message)) < 0){
            perror ("[server]Error: writing to client descriptor -4.\n");
        }
        alert_next_player(id, word, true, session);
        return false;
    }
    else if(!respects_rules(input, id, session)){
        while (1){
            receive = recv(clients[id]->sockfd, word, 25, 0);
            if(receive <= 0){
                perror ("[server]Error: receiving first letter from first player.\n");
                break;
            }
            if(!is_valid(word)){
                strcpy(message, "Server: The word you gave is not valid! You just lost, loser!:");
                if(write(clients[id]->sockfd,  message, strlen(message)) < 0){
                perror ("[server]Error: writing to client descriptor -4.\n");
                }
                alert_next_player(id, word, true, session);
                return false;
            }
            else if(respects_rules(word, id, session)){
                break;
            } 
            bzero(word,25);  
        }
    }else{
        strcpy(word,input);
    }
    if(write(clients[id]->sockfd,  "Great! now we wait...", 22) < 0){
        perror ("[server]Error: writing to client descriptor -4.\n");
    }
    // sprintf(message, "%s has chosen the word: \e[1m%s\e[m !", clients[id]->name, word);
    // send_message(message,id,0);
    // bzero(message, MESSAGE_LENGTH);
    // int last_index = strlen(word) - 1;
    // last_two_letters[session][0] = word[last_index - 1];
    // last_two_letters[session][1] = word[last_index];
    // sprintf(message, "Type a word starting with the last two letters of the previous word: \e[1m%c%c\e[m", last_two_letters[session][0], last_two_letters[session][1]);
    // int next_id = (id + 1) % players_per_session + players_per_session * session;
    // if(write(clients[next_id]->sockfd,  message, strlen(message)) < 0){
    //     perror ("[server]Error: writing to client descriptor -4.\n");
    // }
    // clients[id]->next = false;
    // clients[next_id]->next = true;
    alert_next_player(id, word, false, session);
    return true;
}

void *handle_client(void * arg){
    char message[MESSAGE_LENGTH + NAME_LEN + 1];
    char rcvmessage[MESSAGE_LENGTH];
    char name[NAME_LEN];
    int leave_flag = 0;
    bool keep_playing;

    client_struct *cli = (client_struct*)arg;

    int session_number = cli->id / players_per_session;
    int first_player = session_number * players_per_session;
    int last_player = first_player + players_per_session;

    //setting name
    int player_num = (cli->id % players_per_session) + 1;
    sprintf(name, "Player %d", player_num);
    strcpy(cli->name, name);

    bzero(message, MESSAGE_LENGTH + NAME_LEN + 1);
    sprintf(message, "You are %s", name);
    if( write(cli->sockfd, message, strlen(message)) < 0){
        perror("[server]Error: writing to client descriptor -5.\n");
    }

    bzero(rcvmessage, MESSAGE_LENGTH);

    if((cli->id + 1) % players_per_session == 0){
        sprintf(message,"Server: %s has joined!", cli->name);
        send_message(message, cli->id, 0);
        bzero(message, MESSAGE_LENGTH + NAME_LEN + 1);
        strcpy(message,"The game just started!");
        send_message(message, cli->id, 1);
        begin_session(cli->id, first_player, last_player);
    }else{
        sprintf(message,"Server: %s has joined", cli->name);
        send_message(message, cli->id, 0);
    }
    bzero(message, MESSAGE_LENGTH + NAME_LEN + 1);

    while(1){
        if(leave_flag){
            break;
        }

        int receive = recv(cli->sockfd, rcvmessage, MESSAGE_LENGTH, 0);

        if(receive < 0){
            perror("[server]Error: receiving message from player -1\n");
            leave_flag = 1;
        }else if( receive == 0){
            sprintf(rcvmessage, "%s has left.", cli->name);
            printf("%s.\n",rcvmessage);
            send_message(rcvmessage, cli->id, 0);
            // cli->lost = true;
            // int winner = get_winner(cli->id, session_number);
            // if(winner > -1){
            //     strcpy(message, "Server: Congratulations! You just won!");
            //     if(write(clients[winner]->sockfd,  message, strlen(message)) < 0){
            //         perror ("[server]Error: writing to client descriptor -4.\n");       
            //     }
            // }
            check_unexpected_exit(cli->id, session_number);
            leave_flag = 1;
        }else if(!cli->sesion_started || !cli->next){
            strcpy(rcvmessage, "Server: Game session has not started yet or it's not your turn.");
            if(write(cli->sockfd,  rcvmessage, strlen(rcvmessage)) < 0){
                    perror ("[server]Error: writing to client descriptor - 3.\n");
                    break;
            }
        }else if(last_two_letters[session_number][0] == 0){
            start_game(session_number, cli->id, rcvmessage);
        }else if(strcmp(rcvmessage, "Yay! Get me out") == 0){
            leave_flag = 1;    
        }else{
            keep_playing = play_game(session_number, cli->id, rcvmessage);
            if(!keep_playing){
                leave_flag = 1;
            }
        }
        bzero(rcvmessage, MESSAGE_LENGTH);
        bzero(message, MESSAGE_LENGTH + NAME_LEN+ 1);
    }
    close(cli->sockfd);
    list_remove(cli->id);
    // free(cli);
    list_remove(cli->id);
    // cli_count--;
    pthread_detach(pthread_self());

    return NULL;

}

void get_players_per_session(){
    FILE *info;
    int num;

    info = fopen("players_per_session.txt", "r");
    if(info == NULL){
        perror ("[server]Cant open config file.\n");
    }

    fscanf(info, "%1d", &num);
    players_per_session = num;

    fclose(info);
}


int main(){
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    int sd, client_d;
    pthread_t thread_id;

    //update config and check it
    get_players_per_session();
    if(players_per_session == 0){
        perror ("[server]There cant be a session with 0 players(config info didnt update)\n");
    	return errno;
    }

    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
    	perror ("[server]Socket error.\n");
    	return errno;
    }

    bzero (&server_addr, sizeof (server_addr));
    bzero (&client_addr, sizeof (client_addr));

    //structura socket server
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl (INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    //atasare socket
    if (bind (sd, (struct sockaddr *) &server_addr, sizeof (struct sockaddr)) == -1)
    {
    	perror ("[server]Binding error.\n");
    	return errno;
    }

    if(listen(sd, 10) < 0){
        perror("[server]Listen error.\n");
        return errno;
    }

    printf("~~~~SERVER ON~~~~\nWaiting for players...\n");

    while(1){
        socklen_t client_len = sizeof(client_addr);

    	// fflush (stdout);

        //acceptam clientii
        client_d = accept(sd, (struct sockaddr*)&client_addr, &client_len);

        if((cli_index + 1) == ARRAY_SIZE){
            perror("[server]Server capacity full, reboot needed.\n");
            close(client_d);
            return errno;
        }

        client_struct *player = (client_struct *)malloc(sizeof(client_struct));
        player->id = cli_index;
        player->sockfd = client_d;
        player->next = false;
        player->sesion_started = false;
        player->lost = false;

        list_add(player);
        cli_index++;
        pthread_create(&thread_id, NULL, &handle_client, (void*)player);

        sleep(1);

    }
    exit(0);
}