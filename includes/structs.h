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
    STATUS
} Request_Types;

typedef enum client_info_type {
    FIRST,
    LAST
} Client_Info_Type;

typedef enum server_message_type {
    LISTEN,
    STOP
} Server_Message_Type;

// cliente manda para o servidor
typedef struct client_info {
    int pid;
    char name[20];
    struct timeval time_stamp;
    Client_Info_Type type;
} Client_Info;

// servidor guarda a informação dos processos em ficheiro
typedef struct process_info {
    int active;
    int pid;
    char name[20];
    struct timeval time_stamp_start;
    struct timeval time_stamp_end;
} Process_Info;

// cliente envia request de conexão para o servidor 
typedef struct request {
    int pid;
    char name[30];
    Request_Types type;
} Request;

// servidor envia informação para o cliente
typedef struct server_message {
    int pid;
    char name[20];
    long time;
    Server_Message_Type type;
} Server_Message;