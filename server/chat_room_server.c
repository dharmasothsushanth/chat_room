#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>


#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    char username[BUFFER_SIZE];
}client_arr_t;   //structure using to get all the socketfds and usernames globally

double timeout = 0;          //timeout value
int max_number_streams = 0;       //maximum number of streams
int current_number_of_clients = 0; //number of clients active
bool *is_free;                //whether the thread is free or not
client_arr_t *client_arr;     //array of the structures 
pthread_mutex_t mutex_lock;   //mutex of critical sections

typedef struct {
    int client_socket; 
    int thread_used;           //0 to max_number_streams-1
    char username[BUFFER_SIZE];
    struct sockaddr_in client_addr;
}client_info_t;



void *client_thread_handling(void *arg) 
{

    client_info_t *client = (client_info_t *)arg;

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client->client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

    send(client->client_socket, "write your username: \n", 100, 0);
    char username[BUFFER_SIZE];


    struct timeval timeoutt;
    timeoutt.tv_sec = timeout; 
    timeoutt.tv_usec = 0;

    // Set timeout option on the socket
    if (setsockopt(client->client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeoutt, sizeof(timeoutt)) < 0) 
    {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    int flag = 0;  //for checking if we typed a valid username

    while(flag == 0)
    {

    int bytes_received = recv(client->client_socket, username, BUFFER_SIZE, 0);

    pthread_mutex_lock(&mutex_lock);   //critical section
   
    if(bytes_received <=0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK) 
        {
            char client_ip[INET_ADDRSTRLEN];
            memset(client_ip,'\0',sizeof(client_ip));
            inet_ntop(AF_INET,&client->client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            int client_port = (client->client_addr.sin_port);
            send(client->client_socket, "timeout try later!\n", 100, 0);
            printf("client with ip: %s:%d exited\n",client_ip,client_port);
        } 
        else if(bytes_received<0)
        {
            perror("Recv failed\n");
        }
        else
        {
            char client_ip[INET_ADDRSTRLEN];
            memset(client_ip,'\0',sizeof(client_ip));
            inet_ntop(AF_INET,&client->client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            int client_port = (client->client_addr.sin_port);
            printf("client with ip: %s:%d exited\n",client_ip,client_port);
        }
        close(client->client_socket);
        is_free[client->thread_used]=true;
        pthread_mutex_unlock(&mutex_lock);
        free(client);
        return NULL;

    }

    username[bytes_received] = '\0';

    int flag2 = 1;
    for (int i = 0; i < max_number_streams; ++i) 
    {
        if(!is_free[i] && strcmp(client_arr[i].username, username) == 0 && i!=client->thread_used)
        {
           send(client->client_socket, "username already taken try again!\n", 50, 0);
           flag2 = 0;
           break;
        }
    }

    if(flag2)
    {
        flag = 1;
    }
    else
    {
        pthread_mutex_unlock(&mutex_lock);
    }

    }
   
    
    client_arr[client->thread_used].socket = client->client_socket;
    strcpy(client_arr[client->thread_used].username, username);

    char welcome_msg[BUFFER_SIZE];
    sprintf(welcome_msg, "Welcome to the chatroom, %s!\n", username);  //sendin g welcome message to the user
    send(client->client_socket, welcome_msg, strlen(welcome_msg), 0);
    
    char client_list[BUFFER_SIZE];
    sprintf(client_list, "Connected clients: ");
    for (int i = 0; i < max_number_streams; ++i) {
        if(!is_free[i])
        {
        strcat(client_list, client_arr[i].username);
        strcat(client_list, " ");
        }
    }
    strcat(client_list, "\n");
    send(client->client_socket, client_list, strlen(client_list), 0);

    char user_joined_msg[BUFFER_SIZE];
    sprintf(user_joined_msg, "Client %s joined the chatroom\n", username);    
    printf("Client %s joined the chatroom\n", username);
    for(int i=0;i<max_number_streams;i++)
    {
        if(!is_free[i] && i!=client->thread_used)
        send(client_arr[i].socket, user_joined_msg, strlen(user_joined_msg), 0);   //sending certain user has joined for all other users
    }

    pthread_mutex_unlock(&mutex_lock);


    char buffer[BUFFER_SIZE];
    while(1)
    {
        memset(buffer, '\0', sizeof(buffer));
        
        int bytes_received = recv(client->client_socket, buffer, BUFFER_SIZE, 0);

        if (bytes_received <= 0) 
        {
            pthread_mutex_lock(&mutex_lock);   //critical section

            char user_left_msg[BUFFER_SIZE];
            if (errno == EAGAIN || errno == EWOULDBLOCK)           //checking if timeout has occured
            {
                send(client->client_socket, "you are being kicked out for inactivity\n", 100, 0);
            } 
            else if(bytes_received<0)
            {
                perror("Recv failed\n");
            }

            sprintf(user_left_msg, "Client %s left the chatroom\n", username);
            printf("Client %s left the chatroom\n", username);
    
            for(int i=0;i<max_number_streams;i++)
            {
                if(!is_free[i] && i!=client->thread_used)
                send(client_arr[i].socket, user_left_msg, strlen(user_left_msg), 0);   //sending all users that a client has left the chatroom
            }
            
    
            close(client->client_socket);
            current_number_of_clients--;
            is_free[client->thread_used] = true;
            pthread_mutex_unlock(&mutex_lock);

            free(client);
            return NULL;
        }

        buffer[bytes_received] = '\0';



        if(strcmp(buffer, "\\list") == 0)  //command will print all the current users 
        {
             char client_list[BUFFER_SIZE] = "Connected clients: ";

            pthread_mutex_lock(&mutex_lock);  //critical section
            for (int i = 0; i < max_number_streams; ++i) {
                if(!is_free[i])
                {
                    strcat(client_list, client_arr[i].username);
                    strcat(client_list, " ");
                } 
            }
            strcat(client_list, "\n");
            send(client->client_socket, client_list, strlen(client_list), 0);
            pthread_mutex_unlock(&mutex_lock);
        }
        else if(strcmp(buffer, "\\bye") == 0) //command will make the client leave
        {

            pthread_mutex_lock(&mutex_lock);   //critical section

            char user_left_msg[BUFFER_SIZE];
            sprintf(user_left_msg, "Client %s left the chatroom\n", username);
            printf("Client %s left the chatroom\n", username);
            send(client->client_socket, "You left the chatroom ur input will not be sent\n",100,0);
    
            for(int i=0;i<max_number_streams;i++)
            {
                if(!is_free[i] && i!=client->thread_used)
                send(client_arr[i].socket, user_left_msg, strlen(user_left_msg), 0);
            }

            close(client->client_socket);
            current_number_of_clients--;
            is_free[client->thread_used] = true;
            pthread_mutex_unlock(&mutex_lock);

            free(client);
            return NULL;

        }
        else
        {
            char msg_with_user[BUFFER_SIZE];
            sprintf(msg_with_user, "%s: %s", username, buffer);
            pthread_mutex_lock(&mutex_lock);  //critical section
            for(int i=0;i<max_number_streams;i++)
            {
                if(!is_free[i] && i!=client->thread_used)
                send(client_arr[i].socket, msg_with_user, strlen(msg_with_user), 0);  //sending normal message to all the other users
            }
            pthread_mutex_unlock(&mutex_lock);

        }
    }

    return NULL;

}





int get_first_free_thread()  // will get u the free thread available
{
    for(int i=0;i<max_number_streams;i++)
    {
        if(is_free[i] == true){
            return i;
        }
    }
    return -1;
}



int main(int argc, char * argv[]){
    
    if(argc != 4){
        perror("Invalid Command line arguments\n");
        return 0;
    }

    pthread_mutex_init(&mutex_lock, NULL);



    int port_number = atoi(argv[1]);
    max_number_streams = atoi(argv[2]);
    timeout = atoi(argv[3]);

    is_free = malloc(sizeof(bool) * max_number_streams);
    for(int i=0;i<max_number_streams;i++)
    {
        is_free[i] = true;
    }

    client_arr = malloc(sizeof(client_arr_t) * max_number_streams);

    int server_socket;
    struct sockaddr_in server_addr;
    
    //Creating Socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket == -1){
        perror("Error creating socket\n");
        return 0;
    }

    //Initializing server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    //Binding socket to address and port
    int binding_test = bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(binding_test == -1){
        perror("Error in binding the server socket\n");
        return 0;
    }

    //Listening to the clients
    int listen_test = listen(server_socket, 100); //Queue 100
    if(listen_test == -1){
        perror("Unable to listen on socket\n");
        return 0;
    }

    printf("Server listening on port number %d\n", port_number);

    //Creating threads for our program
    pthread_t threads[max_number_streams];
    
    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;

    while(1){
        while(max_number_streams <= current_number_of_clients){}

        client_addr_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if(client_socket == -1){
            perror("Error in accepting a client\n");
            continue;
        }

        client_info_t *client = (client_info_t *)malloc(sizeof(client_info_t));
        client->client_socket = client_socket;
        memcpy(&(client->client_addr), &client_addr, sizeof(client_addr));

        pthread_mutex_lock(&mutex_lock);   //critical section
        current_number_of_clients++;
        int thread_index = get_first_free_thread(max_number_streams);
        is_free[thread_index] = false;
        client->thread_used = thread_index;
        pthread_create(&threads[thread_index], NULL, client_thread_handling, (void *) client);
        pthread_mutex_unlock(&mutex_lock);
    }

    close(server_socket);
    
    return 0;
}