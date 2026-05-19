#ifndef MICRON_MICRON_TOOLBOX_TIMERS_H
#define MICRON_MICRON_TOOLBOX_TIMERS_H
#include "micron_toolbox_query.h"
#include <stdio.h>

void handle_timer_query(struct query q);
void terminate_timer_query(struct query q);
void terminate_all_timers();
void destroy_all_mutexes();
void display_all_timers();
void dump_all_timers(FILE *f);
void init_timers();

#endif //MICRON_MICRON_TOOLBOX_TIMERS_H
