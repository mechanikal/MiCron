#ifndef LOGGER_LOGGER_H
#define LOGGER_LOGGER_H

#include <stdio.h>

enum log_verbosity{
    MIN = 1,
    STANDARD,
    MAX
};
enum dump_mode{
    HEAP = 1,
    DATA,
    STACK
};
int init_logger_lib(
        void (*dump_heap_f)(FILE *),
        void (*dump_stack_f)(FILE *),
        void (*dump_data_f)(FILE *),
        int signal_dump,
        int signal_log_toggle,
        int signal_log_verbosity,
        char* log_filename
);
void logger_terminate();
void log_message(enum log_verbosity verbosity, char* message);

#endif //LOGGER_LOGGER_H
