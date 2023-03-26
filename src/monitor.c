#include "utils.h"
#include <signal.h>
#include <time.h>

#define MAX_MEM 100
#define SERVER_FIFO_PATH "../tmp/server_fifo"
#define ERROR_LOGS_PATH "../logs/errLogs"
#define REQUEST_LOGS_PATH "../logs/reqLogs"

void error_logger (const char str[], int pid) {
    int fd = open(ERROR_LOGS_PATH, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        perror("[ERROR_LOGGER] open");
    }

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char str_time[20];

    strftime(str_time, sizeof(str_time), "%Y-%m-%d %H:%M:%S", tm);

    char buffer[100];
    sprintf(buffer, "%d\t%s\t%s\n", pid, str_time, str);
    int status = write(fd, buffer, sizeof(buffer));
    if (status < 0) {
        perror("[ERROR_LOGGER] write");
    }
} 

void no_zombies() {
    // quando um processo-filho é criado, emite um sigchld
    struct sigaction sa;
    memset(&sa, 0, sizeof(sigaction));
    sa.sa_handler = SIG_DFL; // default handler
    sa.sa_flags = SA_NOCLDWAIT; // do not make zombies => ability of waiting for children lost

    sigaction(SIGCHLD, &sa, NULL);
}

 long timeval_diff(struct timeval *start, struct timeval *end) {
    long msec = (end->tv_sec - start->tv_sec) * 1000;
    msec += (end->tv_usec - start->tv_usec) / 1000;
    return msec;
}

int store_process (const char path[], Client_Info client_info) {

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        error_logger("[STORE_PROCESSES] Could not open path.", getpid());
        perror("[STORE_PROCESSES] open");
        return -1;
    }

    Process_Info process_info;
    process_info.active = 1;
    process_info.pid    = client_info.pid;
    strcpy(process_info.name, client_info.name);

    process_info.time_stamp_start.tv_sec = client_info.time_stamp.tv_sec;
    process_info.time_stamp_start.tv_usec = client_info.time_stamp.tv_usec;

    write(fd, &process_info, sizeof(Process_Info));

    off_t pos = lseek(fd, 0, SEEK_CUR);
    printf("Register %ld\n", pos / sizeof(Process_Info));

    close(fd);
    return pos / sizeof(Process_Info);
}

int update_process (const char path[], Client_Info client_info) {
    int fd = open(path, O_RDWR, 0644);
    if (fd < 0) {
        error_logger("[UPDATE_PROCESS] Could not open path.", getpid());
        perror("[UPDATE_PROCESS] open");
        return -1;
    }

    int found = 0;
    Process_Info process_info;
    while (read(fd, &process_info, sizeof(Process_Info)) > 0 && !found) {
        if (process_info.active == 1 && process_info.pid == client_info.pid) {
            process_info.active = 0;
            process_info.time_stamp_end.tv_sec  = client_info.time_stamp.tv_sec;
            process_info.time_stamp_end.tv_usec = client_info.time_stamp.tv_usec;

            lseek(fd, - sizeof(Process_Info), SEEK_CUR);
            write(fd, &process_info, sizeof(Process_Info));
            found = 1;
        }
    }

    close(fd);

    if (!found) {
        printf("Process not found.\n");
        return -1;
    } else {
        printf("Processes updated and changed to inactive.\n");
        return 0;
    }
}

void print_inactive_process (const char path[], int pid) {
    int fd = open(path, O_RDWR, 0644);
    if (fd < 0) {
        error_logger("[PRINT_PROCESS] Could not open path.", getpid());
        perror("[PRINT_PROCESS] open");
        return;
    }

    int found = 0;
    Process_Info process_info;
    while (read(fd, &process_info, sizeof(Process_Info)) > 0 && !found) {
        if (process_info.active == 1 && process_info.pid == pid) {
            printf("PID: %d, Name: %s\n", process_info.pid, process_info.name);
            found = 1;
        }
    }
    
    if (!found) printf("Not found.\n");
    close(fd);
}

void listening_fifo (Request request, const char path[]) {

    int flag = 1;
    while (flag) {

        int fd = open(request.name, O_RDWR);
        if (fd < 0) {
            error_logger("[LISTENING_FIFO] Could not open FIFO.", getpid());
            perror("[LISTENING_FIFO] open");
            exit(EXIT_FAILURE);
        }

        Client_Info client_info;
        int bytes = read(fd, &client_info, sizeof(Client_Info));
        
        if (bytes == -1) {
            error_logger("[LISTENING_FIFO] read", getpid());
            perror("[LISTENING_FIFO] read");
            exit(EXIT_FAILURE);
        }

        printf("Data read from %d\n", request.pid);

        if (!strcmp(client_info.type, "info_new")) {
            store_process(path, client_info);
        } else if (!strcmp(client_info.type, "info_last")) {
            update_process(path, client_info);
            flag = 0;
        }

        close(fd);
    }
}

void send_status (Request request, const char path[]) {

    printf("Sending Status\n");

    int fd_storage = open(path, O_RDWR, 0644);
    if (fd_storage < 0) {
        error_logger("[SEND_STATUS] Could not open storage", getpid());
        perror("[SEND_STATUS] open");
        exit(EXIT_FAILURE);
    }

    int fd_pipe = open(request.name, O_WRONLY);
    if (fd_pipe < 0) {
        error_logger("[SEND_STATUS] Could not open FIFO", getpid());
        perror("open");
        exit(EXIT_FAILURE);
    }

    Process_Info process_info;
    while (read(fd_storage, &process_info, sizeof(Process_Info)) > 0) {
        if (process_info.active == 1) {

            Server_Message server_msg;
            server_msg.pid = process_info.pid;
            strcpy(server_msg.name, process_info.name);
            struct timeval current_time;
            gettimeofday(&current_time, NULL);
            server_msg.time = timeval_diff(&process_info.time_stamp_start, &current_time);

            write(fd_pipe, &server_msg, sizeof(Server_Message));
        }
    }

    close(fd_storage);

    Server_Message server_msg;
    server_msg.pid = -1;
    strcpy(server_msg.name, "");
    server_msg.time = 0;

    write(fd_pipe, &server_msg, sizeof(Server_Message));

    printf("[SEND_STATUS] Ended.\n");
    close(fd_pipe);
}

int main (int argc, char const *argv[]) {

    if (argc < 2) return 0;
    char *pids_folder = strdup(argv[1]); 

    time_t start_time = time(NULL);

    printf("Server on.\n");

    if ((mkfifo (SERVER_FIFO_PATH, 0664)) < 0) {
        error_logger("[MONITOR MAIN] mkfifo", getpid());
        perror("[MONITOR MAIN] mkfifo");
        exit(EXIT_FAILURE);
    }

    printf("Server FIFO pipe created.\n\n");

    no_zombies();
    time_t current_time = time(NULL);
    while (!(difftime(current_time, start_time) >= 300)) {

        int fd = open(SERVER_FIFO_PATH, O_RDONLY);
        if (fd < 0) {
            error_logger("[MONITOR MAIN] Could not open server fifo", getpid());
            perror("[MONITOR MAIN] open");
            exit(EXIT_FAILURE);
        }

        Request request;
        int bytes = read(fd, &request, sizeof(Request));

        if (bytes == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        printf("Request from %d received.\n", request.pid);

        int res = fork();
        if (res == 0) {
            if (!strcmp(request.type, "newfifo")) {
                listening_fifo(request, pids_folder);
            } else if (!strcmp(request.type, "status")) {
                send_status(request, pids_folder);
            }
            _exit(0);
        }

        close(fd);
        current_time = time(NULL);
    }

    int status = unlink(SERVER_FIFO_PATH);
    if (status != 0) {
        perror("[MONITOR MAIN] unlink");
        exit(EXIT_FAILURE);
    }

    printf("SERVER FIFO TERMINATED.\n"); 
    free(pids_folder);   
    return 0;
}