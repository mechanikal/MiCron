#include "micron_toolbox_timers.h"
#include "micron_toolbox_query.h"
#include "micron_toolbox_date.h"
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <spawn.h>
#include <string.h>
#include <stdio.h>
#include "logger.h"

#define LOG_MESSAGE_BUFFER 64
#define TASK_LIMIT 50

extern char **environ;

struct task{
    struct query query;
    pthread_mutex_t task_mutex;
};
struct task tasks[TASK_LIMIT];

timer_t create_timer(struct task* t);
void timer_set_relative(struct task* t);
void timer_first_set_absolute(struct task* t);
void timer_tick_thread(union sigval arg);
void timer_set_absolute(struct task* t);
int reset_absolute_timer(struct task* t);

void init_timers(){
    for (int i = 0; i < TASK_LIMIT; ++i) {
        tasks[i].query.active = false;
        pthread_mutex_init(&tasks[i].task_mutex,NULL);
    }
}
timer_t create_timer(struct task* t){
    struct sigevent timer_event = {0};
    timer_event.sigev_notify = SIGEV_THREAD;
    timer_event.sigev_notify_function = timer_tick_thread;
    timer_event.sigev_value.sival_ptr = t;
    timer_event.sigev_notify_attributes = NULL;
    timer_create(CLOCK_REALTIME, &timer_event, &t->query.timer_id);
    return t->query.timer_id;
}
int handle_timer_timeout(struct query query){
    pid_t pid;
    char msg_to_log[LOG_MESSAGE_BUFFER];
    char * filename = query.filename;
    char * args[query.argc+1];
    int err;
    for (int i = 0; i < query.argc; ++i) {
        args[i] = query.program_args[i];
    }
    args[query.argc] = NULL;
    err = posix_spawn(&pid,filename,NULL,NULL,args,environ);
    if(err != 0){
        return 1; // failed to spawn possix
    } else{
        sprintf(msg_to_log,"file %s launched successfully",query.filename);
        log_message(MIN,msg_to_log);
        return 0;
    }
}
void timer_tick_thread(union sigval arg){
    char msg_to_log[LOG_MESSAGE_BUFFER];
    struct task *t = (struct task *) arg.sival_ptr;
    pthread_mutex_lock(&t->task_mutex);
    if (!t->query.active) {
        pthread_mutex_unlock(&t->task_mutex);
        return;
    }
    struct query q_temp = t->query;
    pthread_mutex_unlock(&t->task_mutex);
    int err = handle_timer_timeout(q_temp);
    pthread_mutex_lock(&t->task_mutex);
    if(err!=0){
        sprintf(msg_to_log,"failed to launch file %s",t->query.filename);
        log_message(STANDARD,msg_to_log);
        t->query.active = false;
        pthread_mutex_unlock(&t->task_mutex);
        timer_delete(t->query.timer_id);
        return;
    }
    switch (t->query.scheduleMode) {
        case RELATIVE:
            if(!t->query.cycle){
                t->query.active = false;
                pthread_mutex_unlock(&t->task_mutex);
                timer_delete(t->query.timer_id);
                return;
            } else{
                break;
            }
        case ABSOLUTE:
            if(reset_absolute_timer(t) != 0){
                t->query.active = false;
                pthread_mutex_unlock(&t->task_mutex);
                timer_delete(t->query.timer_id);
                return;
            }
            break;
        default:
            break;
    }
    pthread_mutex_unlock(&t->task_mutex);
}
void timer_from_query(struct task * t){
    t->query.timer_id = create_timer(t);
    switch (t->query.scheduleMode) {
        case RELATIVE:
            timer_set_relative(t);
            log_message(STANDARD,"new relative timer task added");
            break;
        case ABSOLUTE:
            timer_first_set_absolute(t);
            log_message(STANDARD,"new absolute timer task added");
            break;
        default:
            break;
    }
}
void timer_set_relative(struct task* t){
    long seconds = t->query.interval.seconds
            + t->query.interval.minutes * 60
            + t->query.interval.hours * 3600
            + t->query.interval.days * 86400
            + t->query.interval.weeks * 604800;
    struct itimerspec timer_spec = {0};
    timer_spec.it_value.tv_sec = seconds;
    timer_spec.it_interval.tv_sec = seconds*t->query.cycle; // no-repetition timer if cycle is 0
    timer_settime(t->query.timer_id, 0, &timer_spec, NULL);
}
// date already validated
void timer_first_set_absolute(struct task* t){
    struct mi_date template = t->query.miDate;
    time_t now = time(NULL);
    struct mi_date date = date_from_time_t(now);
    date = get_next_date(date,template,t->query.cycleFlags);
    if(date.year == -1){
        log_message(STANDARD,"date in query did not match any future dates");
        t->query.active = 0;
        timer_delete(t->query.timer_id);
        return;
    }
    t->query.miDate = date;
    struct tm date_tm = date_to_tm(date);
    time_t date_time = mktime(&date_tm);
    struct itimerspec timer_spec = {0};
    timer_spec.it_value.tv_sec = date_time;
    timer_settime(t->query.timer_id, TIMER_ABSTIME, &timer_spec, NULL);
}
void timer_set_absolute(struct task* t){
    struct tm date_tm = date_to_tm(t->query.miDate);
    time_t date_time = mktime(&date_tm);
    struct itimerspec timer_spec = {0};
    timer_spec.it_value.tv_sec = date_time;
    timer_settime(t->query.timer_id, TIMER_ABSTIME, &timer_spec, NULL);
}
int reset_absolute_timer(struct task* t){
    struct mi_date d = t->query.miDate;
    increment_date(&d,SECOND);
    struct mi_date next_date = get_next_date(d,t->query.miDate,t->query.cycleFlags);
    if(next_date.year == -1){ // no more dates matching template
        t->query.active = false;
        return 1;
    }
    t->query.miDate = next_date;
    timer_set_absolute(t);
    return 0;
}
void handle_timer_query(struct query q){
    for (int i = 0; i< TASK_LIMIT;i++){
        pthread_mutex_lock(&tasks[i].task_mutex);
        if(tasks[i].query.active == false){
            q.client_id = i;
            q.creation_time = time(NULL);
            tasks[i].query = q;
            timer_from_query(&tasks[i]);
            pthread_mutex_unlock(&tasks[i].task_mutex);
            return;
        }
        pthread_mutex_unlock(&tasks[i].task_mutex);
    }
    // no free timer slots
    log_message(STANDARD,"query was not handled, task limit exceeded");
}
void terminate_timer_query(struct query q){
    char msg_to_log[LOG_MESSAGE_BUFFER];
    if(q.client_id >= TASK_LIMIT || q.client_id < 0){
        log_message(MAX,"invalid task termination id");
        log_message(STANDARD,"task termination failed");
        return;
    }
    pthread_mutex_lock(&tasks[q.client_id].task_mutex);
    if(!tasks[q.client_id].query.active){
        pthread_mutex_unlock(&tasks[q.client_id].task_mutex);
        log_message(MAX,"termination id task slot empty");
        return;
    }
    tasks[q.client_id].query.active = 0;
    pthread_mutex_unlock(&tasks[q.client_id].task_mutex);
    timer_delete(tasks[q.client_id].query.timer_id);
    sprintf(msg_to_log,"task terminated, id: %d",q.client_id);
    log_message(STANDARD,msg_to_log);
}
void terminate_all_timers(){
    struct query q;
    for (int i = 0; i < TASK_LIMIT; i++){
        pthread_mutex_lock(&tasks[i].task_mutex);
        if(tasks[i].query.active){
            q = tasks[i].query;
        } else{
            pthread_mutex_unlock(&tasks[i].task_mutex);
            continue;
        }
        pthread_mutex_unlock(&tasks[i].task_mutex);
        terminate_timer_query(q);
    }
}
void destroy_all_mutexes(){
    for (int i = 0; i< TASK_LIMIT;i++){
        pthread_mutex_destroy(&tasks[i].task_mutex);
    }
}
void show_date_portion(struct query q,enum date_fields df) {
    switch (df) {
        case WEEKDAY:
            if(q.cycleFlags.weekday){
                printf("* ");
            } else{
                printf("%d ",q.miDate.weekday);
            }
            break;
        case YEAR:
            if(q.cycleFlags.year){
                printf("* ");
            } else{
                printf("%d ",q.miDate.year);
            }
            break;
        case MONTH:
            if(q.cycleFlags.month){
                printf("* ");
            } else{
                printf("%d ",q.miDate.month);
            }
            break;
        case DAY:
            if(q.cycleFlags.day){
                printf("* ");
            } else{
                printf("%d ",q.miDate.day);
            }
            break;
        case HOUR:
            if(q.cycleFlags.hour){
                printf("* ");
            } else{
                printf("%d ",q.miDate.hour);
            }
            break;
        case MINUTE:
            if(q.cycleFlags.minute){
                printf("* ");
            } else {
                printf("%d ", q.miDate.minute);
            }
            break;
        case SECOND:
            if(q.cycleFlags.second){
                printf("* ");
            } else{
                printf("%d ",q.miDate.second);
            }
            break;
    }
}
void display_timer_query(struct query q){
    if(!q.active) return;
    switch (q.scheduleMode) {
        case ABSOLUTE:
            printf("id: %d\t",q.client_id);
            printf("schedule mode: absolute\t");
            printf("schedule: ");
            show_date_portion(q,WEEKDAY);
            show_date_portion(q,YEAR);
            show_date_portion(q,MONTH);
            show_date_portion(q,DAY);
            show_date_portion(q,HOUR);
            show_date_portion(q,MINUTE);
            show_date_portion(q,SECOND);
            break;
        case RELATIVE:
            printf("id: %d\t",q.client_id);
            printf("schedule mode: relative\t");
            struct tm creation_tm;
            localtime_r(&q.creation_time,&creation_tm);
            printf("creation date: %04d-%02d-%02d %02d:%02d\t",
                     creation_tm.tm_year + 1900,
                     creation_tm.tm_mon + 1,
                     creation_tm.tm_mday,
                     creation_tm.tm_hour,
                     creation_tm.tm_min
                     );
            printf("interval: ");
            printf("%d weeks, %d days, %d hours, %d minutes, %d seconds. ",q.interval.weeks,q.interval.days,q.interval.hours,q.interval.minutes,q.interval.seconds);
            if(q.cycle){
                printf("CYCLE ");
            } else
                printf("SINGLE-USE ");
            break;
        default:
            return;
    }
    printf("\n task: ");
    for (int i = 0; i < q.argc; ++i) {
        printf("%s ",q.program_args[i]);
    }
    printf("\n");
}
void display_all_timers(){
    int found = 0;
    for (int i = 0; i< TASK_LIMIT;i++){
        pthread_mutex_lock(&tasks[i].task_mutex);
        if(tasks[i].query.active){
            found++;
            display_timer_query(tasks[i].query);
        }
        pthread_mutex_unlock(&tasks[i].task_mutex);
    }
    if(found == 0){
        printf("no current tasks\n");
    }
}
void dump_date_portion(struct query q,enum date_fields df,FILE *f) {
    switch (df) {
        case WEEKDAY:
            if(q.cycleFlags.weekday){
                fprintf(f,"* ");
            } else{
                fprintf(f,"%d ",q.miDate.weekday);
            }
            break;
        case YEAR:
            if(q.cycleFlags.year){
                fprintf(f,"* ");
            } else{
                fprintf(f,"%d ",q.miDate.year);
            }
            break;
        case MONTH:
            if(q.cycleFlags.month){
                fprintf(f,"* ");
            } else{
                fprintf(f,"%d ",q.miDate.month);
            }
            break;
        case DAY:
            if(q.cycleFlags.day){
                fprintf(f,"* ");
            } else{
                fprintf(f,"%d ",q.miDate.day);
            }
            break;
        case HOUR:
            if(q.cycleFlags.hour){
                fprintf(f,"* ");
            } else{
                fprintf(f,"%d ",q.miDate.hour);
            }
            break;
        case MINUTE:
            if(q.cycleFlags.minute){
                fprintf(f,"* ");
            } else {
                fprintf(f,"%d ", q.miDate.minute);
            }
            break;
        case SECOND:
            if(q.cycleFlags.second){
                fprintf(f,"* ");
            } else{
                fprintf(f,"%d ",q.miDate.second);
            }
            break;
    }
}
void dump_timer_query(struct query q,FILE *f){
    if(!q.active) return;
    switch (q.scheduleMode) {
        case ABSOLUTE:
            fprintf(f,"id: %d\t",q.client_id);
            fprintf(f,"schedule mode: absolute\t");
            fprintf(f,"schedule: ");
            dump_date_portion(q,WEEKDAY,f);
            dump_date_portion(q,YEAR,f);
            dump_date_portion(q,MONTH,f);
            dump_date_portion(q,DAY,f);
            dump_date_portion(q,HOUR,f);
            dump_date_portion(q,MINUTE,f);
            dump_date_portion(q,SECOND,f);
            break;
        case RELATIVE:
            fprintf(f,"id: %d\t",q.client_id);
            fprintf(f,"schedule mode: relative\t");
            struct tm creation_tm;
            localtime_r(&q.creation_time,&creation_tm);
            fprintf(f,"creation date: %04d-%02d-%02d %02d:%02d\t",
                   creation_tm.tm_year + 1900,
                   creation_tm.tm_mon + 1,
                   creation_tm.tm_mday,
                   creation_tm.tm_hour,
                   creation_tm.tm_min
            );
            fprintf(f,"interval: ");
            fprintf(f,"%d weeks, %d days, %d hours, %d minutes, %d seconds. ",q.interval.weeks,q.interval.days,q.interval.hours,q.interval.minutes,q.interval.seconds);
            if(q.cycle){
                fprintf(f,"CYCLE ");
            } else
                fprintf(f,"SINGLE-USE ");
            break;
        default:
            return;
    }
    fprintf(f,"\n task: ");
    for (int i = 0; i < q.argc; ++i) {
        fprintf(f,"%s ",q.program_args[i]);
    }
    fprintf(f,"\n");
}
void dump_all_timers(FILE *f){
    int found = 0;
    for (int i = 0; i< TASK_LIMIT;i++){
        pthread_mutex_lock(&tasks[i].task_mutex);
        if(tasks[i].query.active){
            found++;
            dump_timer_query(tasks[i].query,f);
        }
        pthread_mutex_unlock(&tasks[i].task_mutex);
    }
    if(found == 0){
        fprintf(f,"no current tasks\n");
    }
}
