//
// Created by root on 12/30/25.
//

#ifndef MICRON_MICRON_TOOLBOX_QUERY_H
#define MICRON_MICRON_TOOLBOX_QUERY_H

#include <stdbool.h>
#include <time.h>
#include "micron_toolbox_date.h"
#define FILENAME_BUFFER 32
#define PROGRAM_ARG_LEN_BUFFER 32
#define MAX_PROGRAM_ARGS 16
#define REPLY_QUEUE_FILENAME_BUFFER 32

enum schedule_mode{ // also functions as query morde
    ABSOLUTE,
    RELATIVE,
    // special queries
    DISPLAY,
    TERMINATE_TASK,
    TERMINATE_PROGRAM,
    INVALID
};

struct query{
    int client_id; //also serves as query to terminate id
    char filename[FILENAME_BUFFER];
    char program_args[MAX_PROGRAM_ARGS+1][PROGRAM_ARG_LEN_BUFFER];
    int argc;
    enum schedule_mode scheduleMode;
    bool active;
    // RELATIVE
    time_t creation_time;
    struct mi_interval interval;
    bool cycle;
    // ABSOLUTE
    struct mi_date miDate;
    struct cycle_flags cycleFlags;
    timer_t timer_id;
    // DISPLAY
    char reply_queue[REPLY_QUEUE_FILENAME_BUFFER];

};
struct query process_query(int argc,char **argv);

#endif //MICRON_MICRON_TOOLBOX_QUERY_H
