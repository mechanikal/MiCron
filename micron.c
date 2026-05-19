#include <stdio.h>
#include <fcntl.h>
#include <mqueue.h>
#include "micron_toolbox_timers.h"
#include "micron_toolbox_query.h"
#include <pthread.h>
#include "logger.h"
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <signal.h>

#define MICRON_FILENAME "server.bin"
#define QUEUE_NAME "/client_server_queue"
#define TASK_LIST_FILENAME "micron_tasks.txt"

mqd_t server_queue_id;

void exec_server();
void exec_client(int argc,char** argv,mqd_t queue);
void * server_thread(void* args);
void terminate_server();
void handle_display();

int main(int argc,char** argv){
    if(argc == 2 && (strcmp(*(argv+1),"cleanup") == 0)){
        mq_unlink(QUEUE_NAME);
        // terminate_server();
        return 2;
    }
    mqd_t queue = mq_open(QUEUE_NAME, O_WRONLY);
    if(queue != (mqd_t)-1) {
        exec_client(argc, argv, queue);
    } else {
        if(fork() == 0) {
            exec_server();
        }else {
            do {
                queue = mq_open(QUEUE_NAME, O_WRONLY);
                sleep(1);
            } while (queue == (mqd_t) -1);
            exec_client(argc, argv, queue);
        }
    }
}
int setup_queue(mqd_t* queue){
    struct mq_attr attr;
    attr.mq_maxmsg = 50;
    attr.mq_msgsize = sizeof(struct query);
    attr.mq_flags = 0;
    *queue = mq_open (QUEUE_NAME, O_RDWR | O_CREAT, 0666, &attr);
    if(*queue == (mqd_t)-1) {
        return 1;
    }
    return 0;
}
void terminate_server(){
    terminate_all_timers();
    destroy_all_mutexes();
    log_message(MIN,"server closed");
    logger_terminate();
}
void exec_server(){
    mq_unlink(QUEUE_NAME);
    init_timers();
    init_logger_lib(dump_all_timers,dump_all_timers,dump_all_timers,SIGRTMIN,SIGRTMIN+1,SIGRTMIN+2,"micron_logger");
    mqd_t queue;
    if(setup_queue(&queue)){
        log_message(MIN,"failed to setup server");
        logger_terminate();
        return;
    }
    server_queue_id = queue;
    pthread_t pid;
    pthread_create(&pid,NULL,server_thread,NULL);
    log_message(MIN,"server started");
    printf("server pid = %d\n",getpid());
    pthread_join(pid,NULL);
}
void * server_thread(void* args){
    mqd_t queue = server_queue_id;
    size_t buf_size = sizeof(struct query);
    struct mq_attr attr;
    mq_getattr (queue, &attr);
    char buf[buf_size];
    struct query q;
    while (1){
        mq_receive(queue, buf,buf_size,NULL);
        q = *((struct query*)(buf));
        log_message(STANDARD,"message received by server");
        if(q.scheduleMode == TERMINATE_PROGRAM){
            log_message(MAX,"server termination requested");
            terminate_server();
            break;
        }
        switch (q.scheduleMode) {
            case TERMINATE_TASK:
                log_message(MAX,"task termination requested");
                terminate_timer_query(q);
                break;
            case DISPLAY:
                log_message(MAX,"task list display requested");
                handle_display(q.reply_queue);
                break;
            default:
                log_message(MAX,"new task schedule requested");
                handle_timer_query(q);
        }
    }
    mq_close(queue);
    mq_unlink(QUEUE_NAME);
    return NULL;
}
void handle_display(char * reply_queue_name){
    mqd_t reply_queue;
    FILE * f_tasks;
    reply_queue = mq_open (reply_queue_name, O_WRONLY);
    if(reply_queue == (mqd_t)-1) {
        //log error
        return;
    }
    char reply;
    f_tasks = fopen(TASK_LIST_FILENAME,"w");
    if(f_tasks == NULL){
        log_message(MAX,"failed to open task list file");
        reply = 'N';
        mq_send(reply_queue,&reply, sizeof(char),1);
        mq_close(reply_queue);
        return;
    }
    int fd = fileno(f_tasks);
    flock(fd, LOCK_EX);
    dump_all_timers(f_tasks);
    flock(fd, LOCK_UN);
    fclose(f_tasks);
    reply = 'Y';
    mq_send(reply_queue,&reply, sizeof(char),1);
    mq_close(reply_queue);
    log_message(MAX,"updated task list file");
}
void client_read_tasks(){
    FILE * f_tasks;
    char c;
    f_tasks = fopen(TASK_LIST_FILENAME,"r");
    if(f_tasks == NULL){
        printf("failed to open file with task list\n");
        return;
    }
    int fd = fileno(f_tasks);

    flock(fd, LOCK_SH);
    while (!feof(f_tasks)){
        c = fgetc(f_tasks);
        if(feof(f_tasks))break;
        putc(c,stdout);
    }
    flock(fd, LOCK_UN);
    fclose(f_tasks);
    return;
}
void exec_client(int argc,char** argv,mqd_t queue){
    struct mq_attr attr;
    mqd_t reply_queue;
    printf("running as client\n");
    size_t buf_size = sizeof(struct query);
    struct query q = process_query(argc,argv);
    if(q.scheduleMode == INVALID){
        printf("query invalid\n");
        fflush(stdout);
        return;
    }
    if(q.scheduleMode == DISPLAY){
        attr.mq_maxmsg = 1,
        attr.mq_msgsize = sizeof(char),
        attr.mq_flags = 0;
        reply_queue = mq_open(q.reply_queue,O_CREAT| O_RDONLY, 0444, &attr);
        if(reply_queue == (mqd_t)-1) {
            printf("failed to create message queue\n");
            fflush(stdout);
            return;
        }
    }
    char buf[buf_size];
    *((struct query*)buf) = q;
    mq_send(queue,buf,buf_size,1); // equal priority for any query
    mq_close(queue);
    printf("query processed and sent to server\n");
    if(q.scheduleMode == DISPLAY){
        char r = '?';
        mq_receive(reply_queue,&r, sizeof(char ),NULL);
        switch (r) {
            case 'Y':
                client_read_tasks();
                break;
            case 'N':
                printf("server failed to specify task list\n");
                break;
            default:
                printf("invalid reply from server\n");
        }
        mq_close(reply_queue);
        mq_unlink(q.reply_queue);
    }
    fflush(stdout);
}
