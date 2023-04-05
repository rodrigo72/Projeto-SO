#include "structs.h"

#define MAX_COMMANDS 10
#define MAX_ARGS 10
#define MAX_ARG_LENGTH 100
#define SERVER_FIFO_PATH "../tmp/server_fifo"

// calcula a diferença em milisegundos de duas estruturas timeval
long timeval_diff(struct timeval *start, struct timeval *end) {
    long msec = (end->tv_sec - start->tv_sec) * 1000;
    msec += (end->tv_usec - start->tv_usec) / 1000;
    return msec;
}

void send_request (int fd_server, int pid, const char name[], Request_Types type) {
    Request request;
    request.pid = pid;
    strcpy(request.name, name);
    request.type = type;

    write(fd_server, &request, sizeof(Request));
    printf("Request sent.\n");
}

Client_Message create_client_msg (int pid, const char name[], Client_Info_Types type) {
    Client_Message client_message;
    client_message.pid = pid;
    strcpy(client_message.name, name);
    client_message.type = type;
    gettimeofday(&client_message.time_stamp, NULL);

    return client_message;
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

long execute_chained_programs (int fd, char names[], const char *pipeline) {
    
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

    int pid = getpid();
    Client_Message client_message_1 = create_client_msg(pid, names, FIRST);
    write(fd, &client_message_1, sizeof(Client_Message));

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

    Client_Message client_message_2 = create_client_msg(pid, names, LAST);
    write(fd, &client_message_2, sizeof(Client_Message));

    for (int i = 0; i < MAX_COMMANDS; i++) {
        for (int j = 0; j < MAX_ARGS; j++) free(commands[i][j]);
        free(commands[i]);
    }

    free(commands);
    return timeval_diff(&client_message_1.time_stamp, &client_message_2.time_stamp);
}

long my_system (int fd, char name[], const char *string) {

    char **sep = malloc(sizeof(char *) * MAX_ARGS);
    memset(sep, 0, sizeof(char *) * MAX_ARGS);

    char *aux = strdup(string);

    for (int i = 0; aux != NULL && i < MAX_ARGS; i++) {
        char *arg = strsep(&aux, " ");
        sep[i] = malloc(sizeof(char) * strlen(arg));
        strncpy(sep[i], arg, strlen(arg));
    }

    Client_Message client_message_1 = create_client_msg(getpid(), name, FIRST);
    write(fd, &client_message_1, sizeof(Client_Message));

    int fres = fork();
    if (fres == 0) {
        int res = execvp(sep[0], sep);
        printf("Did not execute command (%d).\n", res);
        _exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(fres, &status, 0);
        if (!WIFEXITED(status)) {
            printf("Error.\n");
        }
    }

    Client_Message client_message_2 = create_client_msg(getpid(), name, LAST);
    write(fd, &client_message_2, sizeof(Client_Message));

    for (int i = 0; i < MAX_ARGS; i++) free(sep[i]);
    free(sep);

    return timeval_diff(&client_message_1.time_stamp, &client_message_2.time_stamp);
}

int main (int argc, char const *argv[]) {

    int fd_server = open(SERVER_FIFO_PATH, O_WRONLY);
    if (fd_server < 0) {
        perror("[Main] read server fifo");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server.\n");

    int pid = getpid();
    char pipe_name[30];
    snprintf(pipe_name, 30, "../tmp/%d", pid);

    if (mkfifo(pipe_name, 0664) < 0) {
        perror("[main] mkfifo");
        exit(EXIT_FAILURE);
    }

    if (argc >= 4 && !strcmp(argv[1], "execute") && !strcmp(argv[2], "-u")) {

        char *command = strdup(argv[3]);
        char *token = strtok(command, " "); // command name
        if (!token) return 0;

        send_request(fd_server, pid, pipe_name, EXECUTE);
        close(fd_server);
        printf("Main connection closed.\n");

        int fd_client = open(pipe_name, O_WRONLY);
        if (fd_client < 0) {
            perror("open client fifo");
            exit(EXIT_FAILURE);
        }

        printf("\n-------------------------------------\n");
        printf("Running PID %d\n", pid);
        long time = my_system(fd_client, token, argv[3]);
        printf("\nEnded in %ld milliseconds\n", time);
        printf("-------------------------------------\n\n");
                
        close(fd_client);
        printf("Private FD closed.\n");

        free(command);

    } else if (argc >= 4 && !strcmp(argv[1], "execute") && !strcmp(argv[2], "-p")) {
            
        char names[100];
        get_names(argv[3], names);

        send_request(fd_server, pid, pipe_name, EXECUTE);
        close(fd_server);
        printf("Main connection closed.\n");

        int fd_client = open(pipe_name, O_WRONLY);
        if (fd_client < 0) {
            perror("open client fifo");
            exit(EXIT_FAILURE);
        }

        printf("\n-------------------------------------\n");
        printf("Running PID %d\n", pid);
        long time = execute_chained_programs(fd_client, names, argv[3]);
        printf("\nEnded in %ld milliseconds\n", time);
        printf("-------------------------------------\n\n");
                
        close(fd_client);
        printf("Private FD closed\n");

    } else if (argc >= 2 && !strcmp(argv[1], "status")) {

        send_request(fd_server, pid, pipe_name, STATUS);
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

    } else if (argc >= 3 && !strcmp(argv[1], "stats-time")) {

        send_request(fd_server, pid, pipe_name, STATS_TIME);
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
            printf("Total execution time is %ld ms\n", server_msg.total_time);
        }

        close(fd_client);
    } 

    int status = unlink(pipe_name);
    if (status != 0) {
        perror("unlink");
        exit(EXIT_FAILURE);
    }
    printf("FIFO removed.\n");

    return 0;
}