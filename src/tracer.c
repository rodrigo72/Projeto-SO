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

    if (argc >= 4) {
        if (!strcmp(argv[1], "execute") && !strcmp(argv[2], "-u")) {

            int fd = open(SERVER_FIFO_PATH, O_WRONLY);
            if (fd < 0) {
                perror("open");
                exit(EXIT_FAILURE);
            }

            int pid = getpid();
            char *command = strdup(argv[3]);
            char *token = strtok(command, " ");

            if (token) {
                Client_Info client_info_1;
                client_info_1.pid = pid;
                strcpy(client_info_1.name, token);
                strcpy(client_info_1.type, "new");
                gettimeofday(&client_info_1.time_stamp, NULL);

                write(fd, &client_info_1, sizeof(Client_Info));

                Client_Info client_info_2;
                client_info_2.pid = pid;
                strcpy(client_info_2.name, token);
                strcpy(client_info_2.type, "update");

                printf("Running PID %d\n", pid);
                my_system(argv[3]);
                gettimeofday(&client_info_2.time_stamp, NULL);

                printf("Ended in %ld milliseconds\n", 
                    timeval_diff(&client_info_1.time_stamp, &client_info_2.time_stamp));
                
                write(fd, &client_info_2, sizeof(Client_Info));

                Client_Info client_info_3;
                client_info_3.pid = pid;
                strcpy(client_info_3.name, token);
                strcpy(client_info_3.type, "print");

                write(fd, &client_info_3, sizeof(Client_Info));
            }

            close(fd);
            free(command);
        }
    }

    return 0;
}