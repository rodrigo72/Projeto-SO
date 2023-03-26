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

typedef struct old_client_info {
    int pid;
    char name[20];
    struct timeval time_stamp_start;
    struct timeval time_stamp_end;
} Old_Client_Info;
