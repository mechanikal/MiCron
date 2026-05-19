#include "logger.h"
#include <stdatomic.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <semaphore.h>

enum log_signal_action{
    VERBOSITY_MIN = 1,
    VERBOSITY_STANDARD,
    VERBOSITY_MAX
};

void dump(void (*dump_func)(FILE *),char * filename_base);
void dump_handler(int mode);
void change_verbosity(enum log_signal_action target_verbosity);
void toggle_logging();
void handle_signal(int signal,siginfo_t* info, void *ucontext);
void* signal_polling(void* arg);
void logger_log_message(enum log_verbosity verbosity, char* message);

void * dump_thread_stack(void* arg);
void * dump_thread_heap(void* arg);
void * dump_thread_data(void* arg);


// ------------
// init section
// ------------

atomic_int dump_stack_flag = 0;
atomic_int dump_heap_flag = 0;
atomic_int dump_data_flag = 0;
atomic_int toggle_flag = 0;
atomic_int verbosity_flag = 0;
atomic_int queued_dumps = 0;

static void (*dump_heap_func)(FILE *);
static void (*dump_data_func)(FILE *);
static void (*dump_stack_func)(FILE *);
static int DUMP_SIGNAL;
static int LOG_TOGGLE_SIGNAL;
static int LOG_VERBOSITY_SIGNAL;
static char* log_fname;
sigset_t set;
pthread_t signal_thread;

enum log_signal_action verbosity_level;
pthread_mutex_t verbosity_mutex = PTHREAD_MUTEX_INITIALIZER;
bool init_complete = false;
pthread_mutex_t init_complete_mutex = PTHREAD_MUTEX_INITIALIZER;
bool polling = false;
pthread_mutex_t polling_mutex = PTHREAD_MUTEX_INITIALIZER;
bool logging = false;
pthread_mutex_t logging_mutex = PTHREAD_MUTEX_INITIALIZER;
FILE *log_file;
pthread_mutex_t log_file_mutex = PTHREAD_MUTEX_INITIALIZER;

sem_t flag_handled_sem;
sem_t dump_scheduled_sem;
sem_t dump_complete_sem;


int init_logger_lib(
        void (*dump_heap_f)(FILE *),
        void (*dump_stack_f)(FILE *),
        void (*dump_data_f)(FILE *),
        int signal_dump,
        int signal_log_toggle,
        int signal_log_verbosity,
        char* log_filename
){
    pthread_mutex_lock(&init_complete_mutex);
    if (init_complete){
        pthread_mutex_unlock(&init_complete_mutex);
        return -2;
    }
    pthread_mutex_unlock(&init_complete_mutex);
    // define signals
    if(signal_dump < SIGRTMIN || signal_dump > SIGRTMAX){
        return -1;
    }
    if(signal_log_toggle < SIGRTMIN || signal_log_toggle > SIGRTMAX){
        return -1;
    }
    if(signal_log_verbosity < SIGRTMIN || signal_log_verbosity > SIGRTMAX){
        return -1;
    }
    if(signal_log_verbosity == signal_log_toggle || signal_log_toggle == signal_dump || signal_dump == signal_log_verbosity){
        return -1;
    }
    DUMP_SIGNAL = signal_dump;
    LOG_TOGGLE_SIGNAL = signal_log_toggle;
    LOG_VERBOSITY_SIGNAL = signal_log_verbosity;
    struct sigaction signals;
    signals.sa_flags = SA_SIGINFO;
    signals.sa_sigaction = handle_signal;
    sigemptyset(&signals.sa_mask);
    sigaction(DUMP_SIGNAL, &signals, NULL);
    sigaction(LOG_TOGGLE_SIGNAL, &signals, NULL);
    sigaction(LOG_VERBOSITY_SIGNAL, &signals, NULL);
    // define dump funcs
    dump_heap_func = dump_heap_f;
    dump_stack_func = dump_stack_f;
    dump_data_func = dump_data_f;
    // set start verbosity
    verbosity_level = VERBOSITY_STANDARD;
    // set log filename
    log_fname = calloc(sizeof(char),(strlen(log_filename)+5));
    if(log_fname == NULL){
        return -3;
    }
    strcpy(log_fname,log_filename);
    strcat(log_fname,".txt");
    // start signal polling
    pthread_mutex_lock(&polling_mutex);
    polling = true;
    pthread_mutex_unlock(&polling_mutex);
    sigfillset(&set);
    sigdelset(&set,DUMP_SIGNAL);
    sigdelset(&set,LOG_TOGGLE_SIGNAL);
    sigdelset(&set,LOG_VERBOSITY_SIGNAL);
    pthread_sigmask(SIG_SETMASK, &set,NULL);
    sem_init(&flag_handled_sem,0,0);
    sem_init(&dump_scheduled_sem, 0, 0);
    sem_init(&dump_complete_sem, 0, 0);
    pthread_create(&signal_thread, NULL, signal_polling,NULL);
    pthread_mutex_lock(&init_complete_mutex);
    init_complete = true;
    pthread_mutex_unlock(&init_complete_mutex);
    return 0;
}

void logger_terminate(){
    bool current_logging;
    union sigval val;

    while(atomic_load(&dump_stack_flag) || atomic_load(&dump_data_flag) || atomic_load(&dump_heap_flag)){
        sem_wait(&dump_scheduled_sem);
    }
    while (atomic_load(&toggle_flag) || atomic_load(&verbosity_flag)){
        sem_wait(&flag_handled_sem);
    }
    while(atomic_load(&queued_dumps)){
        sem_wait(&dump_complete_sem);
    }
    pthread_mutex_lock(&polling_mutex);
    polling = false;
    pthread_mutex_unlock(&polling_mutex);
    pthread_mutex_lock(&logging_mutex);
    current_logging = logging;
    pthread_mutex_unlock(&logging_mutex);
    val.sival_int = VERBOSITY_MIN;
    sigqueue(getpid(),LOG_VERBOSITY_SIGNAL,val);
    if (current_logging) {
        toggle_logging();
    }
    pthread_join(signal_thread,NULL);
    free(log_fname);
    atomic_store(&toggle_flag,0);
    atomic_store(&verbosity_flag,0);
    sem_post(&flag_handled_sem);
    sem_destroy(&flag_handled_sem);
    sem_destroy(&dump_scheduled_sem);
    sem_destroy(&dump_complete_sem);
    pthread_mutex_lock(&init_complete_mutex);
    init_complete = false;
    pthread_mutex_unlock(&init_complete_mutex);
}

// ------------
// ctrl section
// ------------

void* signal_polling(void* arg){
    int mode;
    while(1){
        pthread_mutex_lock(&polling_mutex);
        if(!polling){
            pthread_mutex_unlock(&polling_mutex);
            break;
        }
        pthread_mutex_unlock(&polling_mutex);
        if (atomic_load(&toggle_flag)){
            atomic_store(&toggle_flag, 0);
            toggle_logging();
            sem_post(&flag_handled_sem);
        }
        if ((mode = atomic_load(&verbosity_flag))){
            change_verbosity(mode);
            atomic_store(&verbosity_flag, 0);
            sem_post(&flag_handled_sem);
        }
        if (atomic_load(&dump_heap_flag)){
            atomic_fetch_add(&queued_dumps,1);
            pthread_t t;
            pthread_create(&t, NULL, dump_thread_heap, NULL);
            pthread_detach(t);
            atomic_store(&dump_heap_flag, 0);
            sem_post(&dump_scheduled_sem);
        }
        if (atomic_load(&dump_data_flag)){
            atomic_fetch_add(&queued_dumps,1);
            pthread_t t;
            pthread_create(&t, NULL, dump_thread_data, NULL);
            pthread_detach(t);
            atomic_store(&dump_data_flag, 0);
            sem_post(&dump_scheduled_sem);
        }
        if (atomic_load(&dump_stack_flag)){
            atomic_fetch_add(&queued_dumps,1);
            pthread_t t;
            pthread_create(&t, NULL, dump_thread_stack, NULL);
            pthread_detach(t);
            atomic_store(&dump_stack_flag, 0);
            sem_post(&dump_scheduled_sem);
        }
        usleep(1000); // TODO: replace with semaphore
    }
    return NULL;
}

void handle_signal(int signal,siginfo_t* info, void *ucontext){
    int info_int = info->si_value.sival_int;
    if (signal == DUMP_SIGNAL){
        switch (info_int) {
            case HEAP:
                atomic_store(&dump_heap_flag,1);
                break;
            case DATA:
                atomic_store(&dump_data_flag,1);
                break;
            case STACK:
                atomic_store(&dump_stack_flag,1);
        }
    }else if (signal == LOG_TOGGLE_SIGNAL) {
        atomic_store(&toggle_flag,1);
    }else if(signal == LOG_VERBOSITY_SIGNAL) {
        atomic_store(&verbosity_flag, info_int);
    }
}

// ------------
//  log section
// ------------

void change_verbosity(enum log_signal_action target_verbosity){
    char message[31];
    char verbosity_string[10];
    enum log_signal_action current_verbosity;

    pthread_mutex_lock(&verbosity_mutex);
    if(target_verbosity == verbosity_level){
        pthread_mutex_unlock(&verbosity_mutex);
        return;
    }
    verbosity_level = target_verbosity;
    current_verbosity = verbosity_level;
    pthread_mutex_unlock(&verbosity_mutex);
    switch (current_verbosity) {
        case VERBOSITY_MIN:
            strcpy(verbosity_string,"MIN");
            break;
        case VERBOSITY_MAX:
            strcpy(verbosity_string,"MAX");
            break;
        case VERBOSITY_STANDARD:
            strcpy(verbosity_string,"STANDARD");
            break;
        default:
            printf("ERROR:\n unexpected signal info, log - %d\n",target_verbosity);
    }
    sprintf(message, "verbosity changed to %s",verbosity_string);
    logger_log_message(MIN,message);
}
void toggle_logging(){
    bool current_logging;

    pthread_mutex_lock(&logging_mutex);
    current_logging = logging;
    pthread_mutex_unlock(&logging_mutex);

    if(!current_logging){
        pthread_mutex_lock(&log_file_mutex);
        log_file = fopen(log_fname,"a");
        if(log_file == NULL){
            pthread_mutex_unlock(&log_file_mutex);
            return;
        }
        pthread_mutex_unlock(&log_file_mutex);

        pthread_mutex_lock(&logging_mutex);
        logging = true;
        pthread_mutex_unlock(&logging_mutex);
        logger_log_message(STANDARD,"logging started");
    } else{
        logger_log_message(STANDARD,"logging stopped");
        pthread_mutex_lock(&logging_mutex);
        logging = false;
        pthread_mutex_unlock(&logging_mutex);

        pthread_mutex_lock(&log_file_mutex);
        fclose(log_file);
        log_file = NULL;
        pthread_mutex_unlock(&log_file_mutex);
    }
}
void log_message(enum log_verbosity verbosity, char* message){
    struct timeval tv;
    time_t t;
    int ms;
    char time_string[20];
    enum log_verbosity current_verbosity;
    bool current_logging;
    while (atomic_load(&toggle_flag) || atomic_load(&verbosity_flag)){
        sem_wait(&flag_handled_sem);
    }
    pthread_mutex_lock(&logging_mutex);
    current_logging = logging;
    pthread_mutex_unlock(&logging_mutex);
    pthread_mutex_lock(&verbosity_mutex);
    current_verbosity = verbosity_level;
    pthread_mutex_unlock(&verbosity_mutex);
    if(!current_logging || verbosity > current_verbosity){
        return; // message not logged
    }
    gettimeofday(&tv, NULL);
    ms = (int)(tv.tv_usec / 1000);
    time(&t);
    strftime(time_string,20,"%Y.%m.%d %H:%M:%S", localtime(&t));
    pthread_mutex_lock(&log_file_mutex);
    fprintf(log_file,"[%s.%d] %s\n",time_string, ms, message); //write log to file
    pthread_mutex_unlock(&log_file_mutex);
}
void logger_log_message(enum log_verbosity verbosity, char* message){ //same as above but without flag handled sem
    struct timeval tv;
    time_t t;
    int ms;
    char time_string[20];
    enum log_verbosity current_verbosity;
    bool current_logging;

    pthread_mutex_lock(&logging_mutex);
    current_logging = logging;
    pthread_mutex_unlock(&logging_mutex);
    pthread_mutex_lock(&verbosity_mutex);
    current_verbosity = verbosity_level;
    pthread_mutex_unlock(&verbosity_mutex);
    if(!current_logging || verbosity > current_verbosity){
        return; // message not logged
    }
    gettimeofday(&tv, NULL);
    ms = (int)(tv.tv_usec / 1000);
    time(&t);
    strftime(time_string,20,"%Y.%m.%d %H:%M:%S", localtime(&t));
    pthread_mutex_lock(&log_file_mutex);
    fprintf(log_file,"[%s.%d] %s\n",time_string, ms, message); //write log to file
    pthread_mutex_unlock(&log_file_mutex);
}

// ------------
// dump section
// ------------

void dump_handler(int mode){

    switch (mode) {
        case HEAP:
            dump(dump_heap_func,"AppDump_Heap");
            break;
        case DATA:
            dump(dump_data_func,"AppDump_Data");
            break;
        case STACK:
            dump(dump_stack_func,"AppDump_Stack");
            break;
        default:
            printf("ERROR:\n unexpected dump mode\n");
    }
}
void * dump_thread_stack(void* arg){
    dump_handler(STACK);
    atomic_fetch_sub(&queued_dumps,1);
    sem_post(&dump_complete_sem);
    return NULL;
}
void * dump_thread_heap(void* arg){
    dump_handler(HEAP);
    atomic_fetch_sub(&queued_dumps,1);
    sem_post(&dump_complete_sem);
    return NULL;
}
void * dump_thread_data(void* arg){
    dump_handler(DATA);
    atomic_fetch_sub(&queued_dumps,1);
    sem_post(&dump_complete_sem);
    return NULL;
}
void dump(void (*dump_func)(FILE *),char * filename_base){
    FILE *d;
    time_t t;
    char filename[64];
    char time_string[32];

    strcpy(filename,filename_base);
    time(&t);
    strftime(time_string,32,"%y%m%d%H%M%S", localtime(&t));
    strcat(filename, time_string);
    strcat(filename, ".txt");
    d = fopen(filename, "w"); //max one file per second
    if (dump_func != NULL){
        dump_func(d);  //if dump function not specified only create the file
    }else{
        printf("ERROR:\n dump function not specified\n");
    }
    fclose(d);
}
