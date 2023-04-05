#include "structs.h"
#include <signal.h>
#include <time.h>
#include <errno.h>

#define MAX_MEM 100
#define SERVER_FIFO_PATH "../tmp/server_fifo"
#define ERROR_LOGS_PATH "../logs/errLogs.txt"
#define REQUEST_LOGS_PATH "../logs/reqLogs.txt"
#define ACTIVE_PROCESSES_STORAGE "activeProcesses"

// error logs são escritos para um ficheiro errLogs
void error_logger (const char str[], int pid) {

    perror(str);
    int fd = open(ERROR_LOGS_PATH, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        perror("[error_logger] open");
    }

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char str_time[20];

    strftime(str_time, sizeof(str_time), "%Y-%m-%d %H:%M:%S", tm);

    char buffer[200];
    sprintf(buffer, "%d\t%s\t%s: %s\n", pid, str_time, str, strerror(errno));
    int status = write(fd, buffer, strlen(buffer));
    if (status < 0) {
        perror("[error_logger] write");
    }
} 

// request logs são escritos para um ficheiro reqLogs
void request_logger (Request_Types type, int pid) {

    int fd = open(REQUEST_LOGS_PATH, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        error_logger("[request_logger] open", getpid());
    }

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char str_time[20];
    strftime(str_time, sizeof(str_time), "%Y-%m-%d %H:%M:%S", tm);
    
    char buffer[200];

    switch (type) {
        case EXECUTE:
            sprintf(buffer, "%d\t%s\tEXECUTE\n", pid, str_time);
            break;
        case STATUS:
            sprintf(buffer, "%d\t%s\tSTATUS\n", pid, str_time);
            break;
    }

    int status = write(fd, buffer, strlen(buffer));
    if (status < 0) {
        error_logger("[request_logger] write", getpid());
    }
} 

// função que faz com que os processos filho não se tornem zombies
// desse modo, o pai não tem de esperar pelos filhos
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

// Guarda um processo ativo num ficheiro activeProcesses
// Caso encontre uma struct com status = 0, substitui essa struct
// Caso não encontre, adiciona ao final do ficheiro
int store_active_process (const char path[], Client_Message client_message) {

    int fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        error_logger("[store_active_process] open", getpid());
        return -1;
    }

    Stored_Client_Message new_message;
    new_message.status = 1;
    new_message.pid = client_message.pid;
    strcpy(new_message.name, client_message.name);
    new_message.time_stamp.tv_sec = client_message.time_stamp.tv_sec;
    new_message.time_stamp.tv_usec = client_message.time_stamp.tv_usec;

    Stored_Client_Message message;
    int found_empty_slot = 0, bytes = 0;
    while ((bytes = read(fd, &message, sizeof(Stored_Client_Message))) > 0 && !found_empty_slot) {
        if (message.status == 0) {
            lseek(fd, - sizeof(Stored_Client_Message), SEEK_CUR);
            write(fd, &new_message, sizeof(Stored_Client_Message));
            printf("Added to storage (op1).\n");
            found_empty_slot = 1;
        }
    }

    if (bytes < 0) error_logger("[store_active_process] read", getpid());

    if (!found_empty_slot) {
        lseek(fd, 0, SEEK_END);
        write(fd, &new_message, sizeof(Stored_Client_Message));
        printf("Added to storage (op2).\n");
    }

    int num_messages = 0;
    struct stat st;
    if (fstat(fd, &st) == 0) {
        num_messages = st.st_size / sizeof(Stored_Client_Message);
    }
    
    close(fd);
    return num_messages;
}

// Cria um ficheiro na diretoria passada como argumento com uma struct que guarda informação do processo acabado
int new_process_file (const char path[], Stored_Client_Message message_1, Client_Message message_2) {
    int fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        error_logger("[new_process_file] open", getpid());
        return -1;
    }

    Stored_Process_Info stored_process_info;
    strcpy(stored_process_info.name, message_2.name);
    stored_process_info.runtime = timeval_diff(&message_1.time_stamp, &message_2.time_stamp);

    if (write(fd, &stored_process_info, sizeof(Stored_Process_Info)) < 1) {
        error_logger("[new_process_file] write", getpid());
        return -1;
    }

    printf("New process file created.\n");
    return 0;
}

// Muda o status de uma struct com um certo PID para 0 no ficheiro activeProcesses
// Chama a função new_process_file
int update_storage (const char path_storage[], const char path_folder_pid[], Client_Message new_message) {
    int fd = open(path_storage, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        error_logger("[update_storage] open", getpid());
        return -1;
    }

    Stored_Client_Message message;
    int found_pid = 0, bytes = 0;
    while ((bytes = read(fd, &message, sizeof(Stored_Client_Message))) > 0 && !found_pid) {
        if (message.pid == new_message.pid) {
            lseek(fd, - sizeof(Stored_Client_Message), SEEK_CUR);
            message.status = 0;
            write(fd, &message, sizeof(Stored_Client_Message));
            new_process_file(path_folder_pid, message, new_message);
            printf("Storage updated.\n");
            found_pid = 1;
        }
    }

    if (bytes < 0) error_logger("[update_storage] read", getpid());

    close(fd);
    return 1;
}

// Ouve por mensagens do client até o extremo de escrita ser fechado
// Chama a função store_active_process na primeira mensagem
// Chama a função update_storage na segunda mensagem
int req_execute (Request request, const char path_storage[], const char pids_folder[]) {

    printf("Waiting for writer...\n");
    int fd_client = open(request.name, O_RDONLY);
    if (fd_client < 0) {
        error_logger("[req_execute] open client fifo", getpid());
        return -1;
    }
        
    int bytes = 0;
    Client_Message client_message;
    while ((bytes = read(fd_client, &client_message, sizeof(Client_Message))) > 0) {
        printf("Client message read.\n");
        
        char path_folder[50];
        sprintf(path_folder, "../%s/%d" , pids_folder, client_message.pid);

        if (client_message.type == FIRST) {
            store_active_process(path_storage, client_message);
        } else if (client_message.type == LAST) {
            update_storage(path_storage, path_folder, client_message);
        }
    }

    close(fd_client);
    printf("Closed client FD.\n");

    if (bytes < 0) {
        error_logger("[req_execute] read client message", getpid());
        return -1;
    }

    return 0;
}

// Manda mensagens ao cliente com informação acerca dos processos ativos do momento
// Lé o ficheiro que guarda informação de processos ativos: activeProcesses
int req_status (Request request, const char path_storage[]) {
    printf("Opening storage.\n");
    int fd_storage = open(path_storage, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd_storage < 0) {
        error_logger("[req_status] open storage", getpid());
        return -1;
    } 

    printf("Waiting for reader ...\n");
    int fd_client = open(request.name, O_WRONLY);
    if (fd_client < 0) {
        error_logger("[req_status] open client fifo", getpid());
        return -1;
    }

    printf("Sending messages.\n");
    int bytes = 0;
    Stored_Client_Message client_message;
    while ((bytes = read(fd_storage, &client_message, sizeof(Stored_Client_Message))) > 0) {
        if (client_message.status == 1) {
            Server_Message server_msg;
            server_msg.pid = client_message.pid;
            strcpy(server_msg.name, client_message.name);
            struct timeval current_time;
            gettimeofday(&current_time, NULL);
            server_msg.time = timeval_diff(&client_message.time_stamp, &current_time);
            write(fd_client, &server_msg, sizeof(Server_Message));
        }
    }

    close(fd_storage);
    close(fd_client);

    printf("Closed client FD.\n");

    if (bytes < 0) {
        error_logger("[req_status] read active process storage", getpid());
        return -1;
    }

    return 0;
}

int main (int argc, char const *argv[]) {

    if (argc < 2) return 0;
    char *pids_folder = strdup(argv[1]); 

    char path_storage[50];
    sprintf(path_storage, "../%s/%s", pids_folder, ACTIVE_PROCESSES_STORAGE);

    printf("Server on.\n");

    if (mkfifo(SERVER_FIFO_PATH, 0644) < 0) {
        error_logger("[Main] mkfifo", getpid());
        exit(EXIT_FAILURE);
    }

    printf("Server FIFO created.\n");

    no_zombies();
    while (1) {
        printf("Waiting for clients ...\n");
        int fd_server = open(SERVER_FIFO_PATH, O_RDONLY);
        if (fd_server < 0) {
            error_logger("[Main] open server fifo", getpid());
            exit(EXIT_FAILURE);
        }

        int bytes;
        Request request;
        while ((bytes = read(fd_server, &request, sizeof(Request))) > 0) {
            
            int res = fork();
            if (res == 0) {
                request_logger(request.type, getpid());
                printf("Request received.\n");

                if (request.type == EXECUTE) {
                    req_execute(request, path_storage, pids_folder);
                } else if (request.type == STATUS) {
                    req_status(request, path_storage);
                }

                _exit(0);
            }
        }

        if (bytes < 0) {
            error_logger("[Main] read request", getpid());
        }

        close(fd_server);
        printf("\n");
    }

    int status = unlink(SERVER_FIFO_PATH);
    if (status != 0) {
        perror("[MONITOR MAIN] unlink");
        exit(EXIT_FAILURE);
    }

    printf("SERVER FIFO TERMINATED.\n"); 
    free(pids_folder);   
}