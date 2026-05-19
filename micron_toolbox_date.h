//
// Created by root on 12/30/25.
//

#ifndef MICRON_MICRON_TOOLBOX_DATE_H
#define MICRON_MICRON_TOOLBOX_DATE_H
#include <stdbool.h>
#include <time.h>

enum date_fields{
    WEEKDAY,
    YEAR,
    MONTH,
    DAY,
    HOUR,
    MINUTE,
    SECOND
};
enum interval_fields{
    WEEKS,
    DAYS,
    HOURS,
    MINUTES,
    SECONDS
};

struct mi_date{
    int weekday;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};
struct mi_interval{
    int weeks;
    int days;
    int hours;
    int minutes;
    int seconds;
};
struct cycle_flags{
    bool weekday;
    bool year;
    bool month;
    bool day;
    bool hour;
    bool minute;
    bool second;
};

bool date_exists(struct mi_date date,struct cycle_flags cf);
long calculate_time_diff(struct mi_date date_a,struct mi_date date_b);
int calculate_weekday(struct mi_date date);
struct mi_date get_next_date(struct mi_date date, struct mi_date template,struct cycle_flags cf);
struct tm date_to_tm(struct mi_date date);
struct mi_date date_from_time_t(time_t t);
void increment_date(struct mi_date *date,enum date_fields mode);

#endif //MICRON_MICRON_TOOLBOX_DATE_H
