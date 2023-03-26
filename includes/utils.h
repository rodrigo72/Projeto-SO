#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>  
#include <sys/stat.h>
#include <sys/wait.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <sys/time.h>

typedef struct client_info {
    int pid;
    char name[20];
    char type[20];
    struct timeval time_stamp;
} Client_Info;

typedef struct process_info {
    int active;
    int pid;
    char name[20];
    struct timeval time_stamp_start;
    struct timeval time_stamp_end;
} Process_Info;

typedef struct request {
    int pid;
    char type[20];
    char name[30];
} Request;

typedef struct server_message {
    int pid;
    char name[20];
    long time;
} Server_Message;