#include "utils.h"
#include <glib.h>
#include <sys/select.h>

#define MAX_MEM 100
#define SERVER_FIFO_PATH "../tmp/server_fifo"

int create_new_pipe (Client_Info client_info, GHashTable *processes_ht) {
    Client_Info *new_client_info = malloc(sizeof(Client_Info *));
    new_client_info->pid = client_info.pid;
    strcpy(new_client_info->name, client_info.name);
    new_client_info->time_stamp.tv_sec  = client_info.time_stamp.tv_sec;
    new_client_info->time_stamp.tv_usec = client_info.time_stamp.tv_usec;

    int success = g_hash_table_insert(processes_ht, GINT_TO_POINTER(client_info.pid), new_client_info);
    if (!success) return -1;

    char pipe_name[20];
    snprintf(pipe_name, 30, "../tmp/%d", client_info.pid);
    if (mkfifo(pipe_name, S_IRUSR | S_IWUSR) < 0) {
        perror("mkfifo");
        return -1;
    }

    printf("New pipe, and new Client Info entry created.\n");
    return 0;
}

void update_process (
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
    
}

void printf_archived_data (Old_Client_Info old_processes[], int curr) {
    for (int i = 0; i < curr; i++)
        printf("PID: %d; Name: %s\n", old_processes[i].pid, old_processes[i].name);
}

int main (void) {

    printf("Server on.\n");

    GHashTable *processes_ht;
    processes_ht = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);

    int curr = 0, i = 0;
    Old_Client_Info old_processes[MAX_MEM];

    printf("Data structures created.\n");

    if ((mkfifo (SERVER_FIFO_PATH, 0664)) < 0) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    printf("Server FIFO pipe created.\n\n");

    while (1) {

        printf("\tReading ... (%d)\n", i); i++;

        int fd = open(SERVER_FIFO_PATH, O_RDONLY);
        if (fd < 0) {
            perror("open");
            exit(EXIT_FAILURE);
        }

        Client_Info client_info;
        int bytes = read(fd, &client_info, sizeof(Client_Info));
        
        if (bytes == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        Client_Info *found_value = g_hash_table_lookup(processes_ht, GINT_TO_POINTER(client_info.pid));

        if (!strcmp(client_info.type, "new") && found_value == NULL) {
            printf("\tCreating new pipe (%d).\n", client_info.pid);
            create_new_pipe(client_info, processes_ht);
            printf("\n");
        } else if (!strcmp(client_info.type, "status") && found_value != NULL) {
            printf("\tSending status to PID %d.\n", client_info.pid);
            send_current_status(client_info, processes_ht);
            printf("\n");
        } else if (!strcmp(client_info.type, "update") && found_value != NULL) {
            printf("\tUpdating data.\n");
            update_process(client_info, found_value, processes_ht, old_processes, &curr);
            printf("\n");
        } else if (!strcmp(client_info.type, "print")) {
            printf("Printing:\n");
            printf_archived_data(old_processes, curr);
            printf("\n");
        }

        close(fd);
    }
    
    return 0;
}