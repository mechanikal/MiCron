//
// Created by root on 12/30/25.
//

#include "micron_toolbox_query.h"
#include "micron_toolbox_date.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

// .c name, mode, wd, Y, M, D, h, m, s, filename
#define ARGC_ABSOLUTE 10
// .c name, mode, s, m, h, d, w, cyclic, filename
#define ARGC_RELATIVE 9
// .c name, mode, query id
#define ARGC_TERMINATE_QUERY 3
#define MODE_ARG_POS 1
#define INTERVAL_ARG_POS 2
#define CYCLIC_ARG_POS 7
#define FILENAME_ARG_R_POS 8
#define DATE_ARG_START 2
#define FILENAME_ARG_A_POS 9
#define QUERY_TO_TERMINATE_ARG_POS 2

bool actual_zero(char* zero){
    if(strlen(zero) == 1 && *zero == '0') return true;
    return false;
}
void set_field(struct query* q,enum date_fields df,bool cyclic, int val){
    if(cyclic) val = 0;
    switch (df) {
        case WEEKDAY:
            q->miDate.weekday = val;
            q->cycleFlags.weekday = cyclic;
            break;
        case YEAR:
            if(cyclic) val = 2000;
            q->miDate.year = val;
            q->cycleFlags.year = cyclic;
            break;
        case MONTH:
            q->miDate.month = val;
            q->cycleFlags.month = cyclic;
            break;
        case DAY:
            q->miDate.day = val;
            q->cycleFlags.day = cyclic;
            break;
        case HOUR:
            q->miDate.hour = val;
            q->cycleFlags.hour = cyclic;
            break;
        case MINUTE:
            q->miDate.minute = val;
            q->cycleFlags.minute = cyclic;
            break;
        case SECOND:
            q->miDate.second = val;
            q->cycleFlags.second = cyclic;
            break;
        default:
            break;
    }
}
void set_interval(struct query *q, enum interval_fields field,int val){
    switch (field){
        case WEEKS:
            q->interval.weeks = val;
            break;
        case DAYS:
            q->interval.days = val;
            break;
        case HOURS:
            q->interval.hours = val;
            break;
        case MINUTES:
            q->interval.minutes = val;
            break;
        case SECONDS:
            q->interval.seconds = val;
            break;
        default:
            break;
    }
}
void process_query_absolute(struct query* q,int argc, char **argv){
    if(argc < ARGC_ABSOLUTE) return; // program arguments optional
    enum date_fields fields[7] = {WEEKDAY,YEAR,MONTH,DAY,HOUR,MINUTE,SECOND};
    bool cyclic = false;
    int val;
    for (int i = 0; i < 7; ++i) {
        if(strcmp(*(argv+DATE_ARG_START+i),"*") == 0){
            cyclic = true;
        }else {
            cyclic = false;
            val = atoi(*(argv+DATE_ARG_START+i));
            if((val == 0 && !actual_zero(*(argv+DATE_ARG_START+i))) || val < 0){
                return;
            }
        }
        set_field(q,fields[i],cyclic,val);
    }
    if(q->cycleFlags.weekday){
        q->miDate.weekday = calculate_weekday(q->miDate);
    }
    if(!date_exists(q->miDate,q->cycleFlags)) return;
    if(strlen(*(argv+FILENAME_ARG_A_POS)) >= FILENAME_BUFFER){
        return;
    } else{
        strcpy(q->filename,*(argv+FILENAME_ARG_A_POS));
    }
    q->argc = argc - ARGC_ABSOLUTE + 1;
    if(q->argc > MAX_PROGRAM_ARGS) return;
    for (int i = 0; i < q->argc; ++i) {
        char * arg = argv[i+FILENAME_ARG_A_POS]; // start with filename
        if(strlen(arg) >= PROGRAM_ARG_LEN_BUFFER) return;
        strcpy(q->program_args[i],arg);
    }
    q->scheduleMode = ABSOLUTE;
}
int interval_is_zero(struct query * q){
    if(q->interval.seconds == 0 && q->interval.minutes == 0 && q->interval.hours == 0 && q->interval.days == 0 && q->interval.weeks == 0)
        return 1;
    return 0;
}
void process_query_relative(struct query* q,int argc, char **argv){
    int reader;
    enum interval_fields fields[5] = {SECONDS,MINUTES,HOURS,DAYS,WEEKS};
    int limits[5] = {59,59,23,6,20};
    if(argc < ARGC_RELATIVE ) return; // program arguments optional
    for (int i = 0; i < 5; ++i) {
        reader = atoi(*(argv+INTERVAL_ARG_POS+i));
        if(reader <= 0 && !actual_zero(*(argv+INTERVAL_ARG_POS+i))) return;
        if(reader > limits[i]) return;
        set_interval(q,fields[i],reader);
    }
    if(interval_is_zero(q)) return;
    if(actual_zero(*(argv+CYCLIC_ARG_POS))){
        q->cycle = false;
    }else if (atoi(*(argv+CYCLIC_ARG_POS)) == 1){
        q->cycle = true;
    } else{
        return;
    }
    if(strlen(*(argv+FILENAME_ARG_R_POS)) >= FILENAME_BUFFER){
        return;
    } else{
        strcpy(q->filename,*(argv+FILENAME_ARG_R_POS));
    }
    q->argc = argc - ARGC_RELATIVE + 1;
    if(q->argc > MAX_PROGRAM_ARGS) return;
    for (int i = 0; i < q->argc; ++i) {
        char * arg = argv[i+FILENAME_ARG_R_POS]; // start with filename
        if(strlen(arg) >= PROGRAM_ARG_LEN_BUFFER) return;
        strcpy(q->program_args[i],arg);
    }
    q->scheduleMode = RELATIVE;
}
void process_query_terminate_task(struct query* q,int argc,char **argv){
    int query_id;
    if(argc < ARGC_TERMINATE_QUERY){
        return;
    }
    if(actual_zero(*(argv+QUERY_TO_TERMINATE_ARG_POS))){
        q->client_id = 0;
        q->scheduleMode = TERMINATE_TASK;
        return;
    }
    query_id = atoi(*(argv+QUERY_TO_TERMINATE_ARG_POS));
    if(query_id == 0){
        return;
    }
    q->client_id = query_id;
    q->scheduleMode = TERMINATE_TASK;

}
struct query process_query(int argc,char **argv){
    struct query q;
    int s_mode;
    q.scheduleMode = INVALID;
    if(argc < 2) return q;
    if(actual_zero(*(argv+MODE_ARG_POS))) {
        s_mode = ABSOLUTE;
    } else{
        s_mode = atoi(*(argv+MODE_ARG_POS));
        if(s_mode == 0){
            return q; // invalid mode
        }
    }
    switch (s_mode) {
        case ABSOLUTE:
            process_query_absolute(&q,argc,argv);
            break;
        case RELATIVE:
            process_query_relative(&q,argc,argv);
            break;
        case TERMINATE_TASK:
            process_query_terminate_task(&q,argc,argv);
            break;
        case TERMINATE_PROGRAM:
            q.scheduleMode = TERMINATE_PROGRAM;
            break;
        case DISPLAY:
            q.scheduleMode = DISPLAY;
            sprintf(q.reply_queue,"/mq_queue_%d",getpid());
            break;
        default:
            return q;
    }
    q.active = true;
    return q;
}
