#include "structs.h"

#define MAX_COMMANDS 10
#define MAX_ARGS 10
#define MAX_ARG_LENGTH 100
#define SERVER_FIFO_PATH "../tmp/server_fifo"

void execute_chained_processes (const char *pipeline) {
    
    int num_cmds = 0;
    char ***commands = malloc(MAX_COMMANDS * sizeof(char **));

    for (int i = 0; i < MAX_COMMANDS; i++) {
        commands[i] = malloc(sizeof(char *) * MAX_ARGS);
        for (int j = 0; j < MAX_ARGS; j++) {
            commands[i][j] = malloc(sizeof(char) * MAX_ARG_LENGTH);
            commands[i][j] = NULL;
        }
    }
    
    char *aux = strdup(pipeline);

    for (int i = 0; aux != NULL && i < MAX_COMMANDS; i++) {
        char *cmd = strsep(&aux, "|");
        int len = strlen(cmd);
        if (cmd == NULL || len == 0) break;

        int k = 0;
        for (int j = 0; j < len && j < MAX_ARGS; j++) {

            while (cmd[k] && cmd[k] == ' ') k++;

            if (cmd[k] == '"') {
                int k2 = k + 1;
                while (k2 < len && cmd[k2] != '"') k2++;
                commands[i][j] = strndup(cmd + k + 1, k2 - k - 1);
                k = k2 + 1;
            } else if (cmd[k] == '\'') {
                int k2 = k + 1;
                while (k2 < len && cmd[k2] != '\'') k2++;
                commands[i][j] = strndup(cmd + k + 1, k2 - k - 1);
                k = k2 + 1;
            } else if (cmd[k] != '\0'){
                int k2 = k + 1;
                while (k2 < len && cmd[k2] != ' ') k2++;
                commands[i][j] = strndup(cmd + k, k2 - k);
                k = k2;
            }
        }
        num_cmds++;
    }

    int pipes[num_cmds-1][2];
    for (int i = 0; i < num_cmds-1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < num_cmds; i++) {
        int pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {
            // Child process
            if (i != 0) { // existe um comando anterior
                dup2(pipes[i-1][0], STDIN_FILENO); // o que entra no pipe é do standard output, i.e. o ouptput do programa anterior é o input deste
                close(pipes[i-1][1]); // fechar write end do pipe do comando anterior
            }
            if (i != num_cmds-1) { // existe um próximo comando
                dup2(pipes[i][1], STDOUT_FILENO); // o input do proximo programa é o output deste
                close(pipes[i][0]); // fechar read end to pipe atual
            }
            // executar comando
            execvp(commands[i][0], commands[i]);
            perror("execvp");
            _exit(EXIT_FAILURE);
        }
        if (i != 0) close(pipes[i-1][0]); // fechar read end to pipe anterior
        if (i != num_cmds-1) close(pipes[i][1]); // fechar write end do pipe atual
    }
    
    // Wait for all child processes to finish
    for (int i = 0; i < num_cmds; i++) {
        int status;
        wait(&status);
        if (!WIFEXITED(status)) {
            printf("Error.\n");
        }
    }

    for (int i = 0; i < MAX_COMMANDS; i++) {
        for (int j = 0; j < MAX_ARGS; j++) {
            free(commands[i][j]);
        }
        free(commands[i]);
    }
    free(commands);
}

void get_names (const char* pipeline, char names[]) {
    char* aux = strdup(pipeline);

    for (int i = 0; aux != NULL && i < MAX_COMMANDS; i++) {
        char* cmd = strsep(&aux, "|");
        int len = strlen(cmd);
        if (cmd == NULL || len == 0) {
            break;
        }

        char* arg = strtok(cmd, " ");
        if (arg != NULL) {
            strcat(names, arg);
            strcat(names, " | ");
        }
    }

    int names_len = strlen(names);
    if (names_len >= 3 && strcmp(names + names_len - 3, " | ") == 0) {
        names[names_len - 3] = '\0';
    }
}

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

    if (argc >= 4 && !strcmp(argv[1], "execute") && !strcmp(argv[2], "-u")) {

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

    } else if (argc >= 4 && !strcmp(argv[1], "execute") && !strcmp(argv[2], "-p")) {
            
        char names[100];
        get_names(argv[3], names);

        int pid = getpid();
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
        strcpy(client_message_1.name, names);
        client_message_1.type = FIRST;
        gettimeofday(&client_message_1.time_stamp, NULL);

        write(fd_client, &client_message_1, sizeof(Client_Message));
        printf("Info sent.\n");

        Client_Message client_message_2;
        client_message_2.pid = pid;
        strcpy(client_message_2.name, names);
        client_message_2.type = LAST;

        printf("\n-------------------------------------\n");
        printf("Running PID %d\n", pid);
        execute_chained_processes(argv[3]);
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
    } else if (argc >= 2 && !strcmp(argv[1], "status")) {

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

    } else if (argc >= 3 && !strcmp(argv[1], "stats-time")) {

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
        request.type = STATS_TIME;

        write(fd_server, &request, sizeof(Request));
        printf("Request sent.\n");

        close(fd_server);
        printf("Main connection closed.\n");

        int fd_client = open(pipe_name, O_WRONLY);
        if (fd_client < 0) {
            perror("open client fifo");
            exit(EXIT_FAILURE);
        }

        for (int i = 2; i < argc; i++) {
            Client_Message_PID client_msg;
            client_msg.pid = atoi(argv[i]);
            write(fd_client, &client_msg, sizeof(Client_Message_PID));
            printf("Message sent.\n");
        }

        close(fd_client);

        fd_client = open(pipe_name, O_RDONLY);
        if (fd_client < 0) {
            perror("open client fifo");
            exit(EXIT_FAILURE);
        }

        Server_Message_Total_Time server_msg;
        int bytes = read(fd_client, &server_msg, sizeof(Server_Message_Total_Time));
        if (bytes < 0) {
            perror("read");
        } else {
            printf("Total execution time is %ld ms", server_msg.total_time);
        }

        close(fd_client);

        int status = unlink(pipe_name);
        if (status != 0) {
            perror("unlink");
            exit(EXIT_FAILURE);
        }
        printf("\nFIFO removed.\n");
    } 

    return 0;
}