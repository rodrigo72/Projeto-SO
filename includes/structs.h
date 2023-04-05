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

typedef enum request_types {
    EXECUTE,
    STATUS,
    STATS_TIME
} Request_Types;

typedef enum client_info_type {
    FIRST,
    LAST
} Client_Info_Types;

typedef struct request {
    int pid;
    char name[20];
    Request_Types type;
} Request;

typedef struct client_message {
    int pid;
    char name[100];
    struct timeval time_stamp;
    Client_Info_Types type;
} Client_Message;

typedef struct stored_client_message {
    int pid;
    char name[100];
    struct timeval time_stamp;
    int status;
} Stored_Client_Message;

typedef struct stored_process_info {
    char name[100];
    long runtime;
} Stored_Process_Info;

typedef struct server_message {
    int pid;
    char name[100];
    long time;
} Server_Message;

typedef struct client_message_pid {
    int pid;
} Client_Message_PID;

typedef struct server_message_total_time {
    long total_time;
} Server_Message_Total_Time;