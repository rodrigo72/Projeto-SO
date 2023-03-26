#include "utils.h"
#include <glib.h>
#include <signal.h>

#define MAX_MEM 100
#define SERVER_FIFO_PATH "../tmp/server_fifo"

int create_new_entry (Client_Info client_info, GHashTable *processes_ht) {
    Client_Info *new_client_info = malloc(sizeof(Client_Info *));
    new_client_info->pid = client_info.pid;
    strcpy(new_client_info->name, client_info.name);
    new_client_info->time_stamp.tv_sec  = client_info.time_stamp.tv_sec;
    new_client_info->time_stamp.tv_usec = client_info.time_stamp.tv_usec;

    int success = g_hash_table_insert(processes_ht, GINT_TO_POINTER(client_info.pid), new_client_info);
    if (!success) return -1;

    return 0;
}

void store_client_info (
    Client_Info client_info_new, 
    Client_Info *client_info_old, 
    GHashTable *processes_ht, 
    Old_Client_Info old_processes[],
    int *curr) {

    Old_Client_Info archive;
    archive.pid = client_info_new.pid;
    strcpy(archive.name, client_info_new.name);

    archive.time_stamp_start.tv_sec  = client_info_old->time_stamp.tv_sec;
    archive.time_stamp_start.tv_usec = client_info_old->time_stamp.tv_usec;

    archive.time_stamp_end.tv_sec  = client_info_new.time_stamp.tv_sec;
    archive.time_stamp_end.tv_usec = client_info_new.time_stamp.tv_usec;

    old_processes[*curr] = archive;
    (*curr)++;

    g_hash_table_remove(processes_ht, GINT_TO_POINTER(client_info_new.pid));
    printf("Entry removed from GHashTable; Client info archived\n");
}

void send_current_status (Client_Info client_info, GHashTable *processes_ht) {
    struct timeval curr_time_stamp;
    gettimeofday(&curr_time_stamp, NULL);
}

void printf_archived_data (Old_Client_Info old_processes[], int curr) {
    for (int i = 0; i < curr; i++)
        printf("PID: %d; Name: %s\n", old_processes[i].pid, old_processes[i].name);
}

void no_zombies() {
    // every time a child exits it emits a sigchild
    struct sigaction sa;
    memset(&sa, 0, sizeof(sigaction));
    sa.sa_handler = SIG_DFL; // default handler
    sa.sa_flags = SA_NOCLDWAIT; // dont make zombies => ability of waiting for children lost

    sigaction(SIGCHLD, &sa, NULL);
}

void send_status (Client_Info client_info, GHashTable *processes_ht) {
    
}

void listening_fifo (Request request, GHashTable *processes_ht, Old_Client_Info old_processes[], int *curr) {

    int flag = 1;
    while (flag) {

        int fd = open(request.name, O_RDWR);
        if (fd < 0) {
            printf("Could not open fifo %d", request.pid);
            perror("open");
            exit(EXIT_FAILURE);
        }

        Client_Info client_info;
        int bytes = read(fd, &client_info, sizeof(Client_Info));
        
        if (bytes == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        printf("Data read from %d\n", request.pid);

        Client_Info *found_value = g_hash_table_lookup(processes_ht, GINT_TO_POINTER(client_info.pid));

        if (!strcmp(client_info.type, "info_new") && found_value == NULL) {
            if (create_new_entry(client_info, processes_ht) == -1) printf("Error creating new entry.\n");
            else printf("New Client Info entry created.\n");
        } else if (!strcmp(client_info.type, "info_last") && found_value != NULL) {
            store_client_info(client_info, found_value, processes_ht, old_processes, curr);
            flag = 0;
        } else if (!strcmp(client_info.type, "status")) {
            send_status(client_info, processes_ht);
        }

        close(fd);
    }
}

int main (void) {

    time_t start_time = time(NULL);

    printf("Server on.\n");

    GHashTable *processes_ht;
    processes_ht = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);

    int curr = 0;
    Old_Client_Info old_processes[MAX_MEM];

    printf("Data structures created.\n");

    if ((mkfifo (SERVER_FIFO_PATH, 0664)) < 0) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    printf("Server FIFO pipe created.\n\n");

    no_zombies();
    time_t current_time = time(NULL);
    while (!(difftime(current_time, start_time) >= 300)) {

        int fd = open(SERVER_FIFO_PATH, O_RDONLY);
        if (fd < 0) {
            printf("Could not open server fifo\n");
            perror("open");
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
                listening_fifo(request, processes_ht, old_processes, &curr);
            }
            _exit(0);
        }

        close(fd);
        current_time = time(NULL);
    }

    int status = unlink(SERVER_FIFO_PATH);
            if (status != 0) {
                perror("unlink");
                exit(EXIT_FAILURE);
            }

    printf("SERVER FIFO TERMINATED.\n");    
    
    return 0;
}