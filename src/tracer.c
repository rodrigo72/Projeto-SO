#include "structs.h"

#define MAX_ARGS 10
#define SERVER_FIFO_PATH "../tmp/server_fifo"

int my_system (const char *string) {

    char **sep = malloc(sizeof(char *) * MAX_ARGS);
    memset(sep, 0, sizeof(char *) * MAX_ARGS);

    char *aux = strdup(string);

    for (int i = 0; aux != NULL && i < MAX_ARGS; i++) {
        char *arg = strsep(&aux, " ");
        sep[i] = malloc(sizeof(char) * strlen(arg));
        strncpy(sep[i], arg, strlen(arg));
    }

    int fres = fork(), r = 1;
    if (fres == 0) {
        int res = execvp(sep[0], sep);
        printf("Did not execute command (%d).\n", res); r = -1;
        _exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(fres, &status, 0);
        if (!WIFEXITED(status)) {
            printf("Error.\n"); r = -1;
        }
    }

    for (int i = 0; i < MAX_ARGS; i++) free(sep[i]);
    free(sep);

    return r;
}

// calcula a diferença em milisegundos de duas estruturas timeval
long timeval_diff(struct timeval *start, struct timeval *end) {
    long msec = (end->tv_sec - start->tv_sec) * 1000;
    msec += (end->tv_usec - start->tv_usec) / 1000;
    return msec;
}

int main (int argc, char const *argv[]) {

    if (argc >= 4) {
        if (!strcmp(argv[1], "execute") && !strcmp(argv[2], "-u")) {

            int pid = getpid();
            char *command = strdup(argv[3]);
            char *token = strtok(command, " "); // command name

            if (!token) return 0;

            char pipe_name[30];
            snprintf(pipe_name, 30, "../tmp/%d", pid);

            int fd_server = open(SERVER_FIFO_PATH, O_WRONLY);
            if (fd_server < 0) {
                perror("[Main] read server fifo");
                exit(EXIT_FAILURE);
            }

            printf("Connected to server.\n");

            if (mkfifo(pipe_name, 0664) < 0) {
                perror("[Main] mkfifo");
                exit(EXIT_FAILURE);
            }

            Request request;
            request.pid = getpid();
            strcpy(request.name, pipe_name);
            request.type = EXECUTE;

            write(fd_server, &request, sizeof(Request));
            printf("Request sent.\n");

            close(fd_server);
            printf("Main connection closed.\n");

            int fd_client = open(pipe_name, O_WRONLY);
            if (fd_client < 0) {
                perror("open client fifo");
                exit(EXIT_FAILURE);
            }

            Client_Message client_message_1;
            client_message_1.pid = pid;
            strcpy(client_message_1.name, token);
            client_message_1.type = FIRST;
            gettimeofday(&client_message_1.time_stamp, NULL);

            write(fd_client, &client_message_1, sizeof(Client_Message));
            printf("Info sent.\n");

            Client_Message client_message_2;
            client_message_2.pid = pid;
            strcpy(client_message_2.name, token);
            client_message_2.type = LAST;

            printf("\n-------------------------------------\n");
            printf("Running PID %d\n", pid);
            my_system(argv[3]);
            gettimeofday(&client_message_2.time_stamp, NULL);

            printf("Ended in %ld milliseconds\n", 
                timeval_diff(&client_message_1.time_stamp, &client_message_2.time_stamp));
            printf("\n-------------------------------------\n");
                
            write(fd_client, &client_message_2, sizeof(Client_Message));
            printf("Info sent.\n");
            close(fd_client);
            printf("Private FD closed\n");

            int status = unlink(pipe_name);
            if (status != 0) {
                perror("unlink");
                exit(EXIT_FAILURE);
            }

            printf("FIFO removed.\n");
            free(command);
        }
    } else if (argc >= 2) {
        if (!strcmp(argv[1], "status")) {

            char pipe_name[30];
            snprintf(pipe_name, 30, "../tmp/%d", getpid());

            int fd_server = open(SERVER_FIFO_PATH, O_WRONLY);
            if (fd_server < 0) {
                perror("[Main] read server fifo");
                exit(EXIT_FAILURE);
            }

            printf("Connected to server.\n");

            if (mkfifo(pipe_name, 0664) < 0) {
                perror("[Main] mkfifo");
                exit(EXIT_FAILURE);
            }

            Request request;
            request.pid = getpid();
            strcpy(request.name, pipe_name);
            request.type = STATUS;

            write(fd_server, &request, sizeof(Request));
            printf("Request sent.\n");

            close(fd_server);
            printf("Main connection closed.\n");

            int fd_client = open(pipe_name, O_RDONLY);
            if (fd_client < 0) {
                perror("open client fifo");
                exit(EXIT_FAILURE);
            }

            printf("Receiving data...\n");
            int bytes = 0;
            Server_Message server_msg;
            while ((bytes = read(fd_client, &server_msg, sizeof(Server_Message))) > 0) {
                printf("%d %s %ld ms\n", server_msg.pid, server_msg.name, server_msg.time);
            }
            
            if (bytes < 0) perror("read server messages");
            close(fd_client);

            int status = unlink(pipe_name);
            if (status != 0) {
                perror("unlink");
                exit(EXIT_FAILURE);
            }
            printf("\nFIFO removed.\n");
        }
    }

    return 0;
}