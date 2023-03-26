#include "utils.h"

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

long timeval_diff(struct timeval *start, struct timeval *end) {
    long msec = (end->tv_sec - start->tv_sec) * 1000;
    msec += (end->tv_usec - start->tv_usec) / 1000;
    return msec;
}

int main (int argc, char const *argv[]) {

    int fd_server = open(SERVER_FIFO_PATH, O_WRONLY);
    if (fd_server < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server.\n\n");

    if (argc >= 4) {
        if (!strcmp(argv[1], "execute") && !strcmp(argv[2], "-u")) {

            int pid = getpid();
            char *command = strdup(argv[3]);
            char *token = strtok(command, " ");

            if (!token) return 0;

            char pipe_name[30];
            snprintf(pipe_name, 30, "../tmp/%d", pid);

            if (mkfifo(pipe_name, 0664) < 0) {
                perror("mkfifo");
                exit(EXIT_FAILURE);
            }

            printf("FIFO pipe created.\n");

            Request request;
            request.pid = pid;
            strcpy(request.type, "newfifo");
            strcpy(request.name, pipe_name);

            write(fd_server, &request, sizeof(Request)); // request for new pipe
            close(fd_server);

            printf("Request sent.\n");

            int fd_client = open(pipe_name, O_WRONLY);
            if (fd_client < 0) {
                perror("open");
                exit(EXIT_FAILURE);
            }

            printf("Private connection established.\n");
            
            Client_Info client_info_1;
            client_info_1.pid = pid;
            strcpy(client_info_1.name, token);
            strcpy(client_info_1.type, "info_new");
            gettimeofday(&client_info_1.time_stamp, NULL);

            write(fd_client, &client_info_1, sizeof(struct client_info));

            printf("Data sent.\n\n");

            Client_Info client_info_2;
            client_info_2.pid = pid;
            strcpy(client_info_2.name, token);
            strcpy(client_info_2.type, "info_last");

            printf("Running PID %d\n", pid);
            my_system(argv[3]);
            gettimeofday(&client_info_2.time_stamp, NULL);

            printf("Ended in %ld milliseconds\n", 
                timeval_diff(&client_info_1.time_stamp, &client_info_2.time_stamp));
                
            write(fd_client, &client_info_2, sizeof(Client_Info));
            close(fd_client);

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

            int pid = getpid();
            char pipe_name[30];
            snprintf(pipe_name, 30, "../tmp/%d", pid);

            if (mkfifo(pipe_name, 0664) < 0) {
                perror("mkfifo");
                exit(EXIT_FAILURE);
            }

            printf("FIFO pipe created.\n");

            Request request;
            request.pid = pid;
            strcpy(request.type, "status");
            strcpy(request.name, pipe_name);

            write(fd_server, &request, sizeof(Request)); // request for new pipe
            close(fd_server);

            printf("Request sent.\n");

            int fd_client = open(pipe_name, O_RDONLY);
            if (fd_client < 0) {
                perror("open");
                exit(EXIT_FAILURE);
            }

            int flag = 1;
            while (flag) {

                Server_Message server_msg;
                int bytes = read(fd_client, &server_msg, sizeof(Server_Message));
                if (bytes == -1) {
                    perror("read");
                    exit(EXIT_FAILURE);
                }

                if (server_msg.pid != -1) {
                    printf("%d %s %ld ms\n", server_msg.pid, server_msg.name, server_msg.time);
                } else {
                    printf("Exiting.\n");
                    flag = 0;
                }

            }

            close(fd_client);

            int status = unlink(pipe_name);
            if (status != 0) {
                perror("unlink");
                exit(EXIT_FAILURE);
            }

            printf("FIFO removed.\n");
        }
    }

    return 0;
}