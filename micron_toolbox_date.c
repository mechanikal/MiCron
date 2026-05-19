//
// Created by root on 12/30/25.
//

#include "micron_toolbox_date.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#define MAX_YEAR 2200

bool is_leap_year(int year){
    if(year%4 == 0 && (year%100 != 0 || year%400 == 0)){
        return true;
    }
    return false;
}
int calculate_weekday(struct mi_date date){ // Zellers's congruence
    int zellers_months[12] = {13,14,3,4,5,6,7,8,9,10,11,12};
    int h;
    int Y = date.year;
    if(date.month < 2){ // jan = 0, feb = 1
        Y-=1;
    }
    int K = Y % 100;
    int J = Y/100;
    int m = date.month;
    int q = date.day;
    h = (q + (13*(zellers_months[m] + 1)/5 + K + K/4 + J/4 + 5*J)) % 7;
    return (h+5) % 7;
}
struct tm date_to_tm(struct mi_date date){
    struct tm date_tm = {0};
    date_tm.tm_year = date.year - 1900;
    date_tm.tm_mon = date.month;
    date_tm.tm_mday = date.day;
    date_tm.tm_hour = date.hour;
    date_tm.tm_min = date.minute;
    date_tm.tm_sec = date.second;
    date_tm.tm_isdst = -1;
    return date_tm;
}
bool date_exists(struct mi_date date,struct cycle_flags cf){
    int month_day[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if(cf.year || is_leap_year(date.year)) month_day[1]++;
    if(!cf.second) {
        if (date.second < 0 || date.second > 59) {
            return false;
        }
    }
    if(!cf.minute) {
        if (date.minute < 0 || date.minute > 59) {
            return false;
        }
    }
    if(!cf.hour) {
        if (date.hour < 0 || date.hour > 23) {
            return false;
        }
    }
    if(!cf.month) {
        if (date.month < 0 || date.month > 11) {
            return false;
        }
    }
    if(!cf.day) {
        if(!cf.month) {
            if (date.day < 1 || date.day > month_day[date.month]) {
                return false;
            }
        } else{
            if (date.day < 1 || date.day > 31) {
                return false;
            }
        }
    }
    if(!cf.weekday){
        if(date.weekday < 0 || date.weekday > 6){
            return false;
        }
        if(!cf.year && !cf.month && !cf.day){
            if(date.weekday != calculate_weekday(date)){
                return false;
            }
        }
    }
    return true;
}
long calculate_time_diff(struct mi_date date_a,struct mi_date date_b){
    struct tm tm_a = date_to_tm(date_a);
    struct tm tm_b = date_to_tm(date_b);
    time_t time_a = mktime(&tm_a);
    time_t time_b = mktime(&tm_b);
    long time_difference = (long)difftime(time_b,time_a);
    return time_difference;
}
bool date_fits_template(struct mi_date date, struct mi_date template,struct cycle_flags cf){
    if(!cf.weekday){
        if(date.weekday != template.weekday) return false;
    }
    if(!cf.year){
        if(date.year != template.year) return false;
    }
    if(!cf.month){
        if(date.month != template.month) return false;
    }
    if(!cf.day){
        if(date.day != template.day) return false;
    }
    if(!cf.hour){
        if(date.hour != template.hour) return false;
    }
    if(!cf.minute){
        if(date.minute != template.minute) return false;
    }
    if(!cf.second){
        if(date.second != template.second) return false;
    }
    return true;
}
void increment_date(struct mi_date *date,enum date_fields mode){
    if(date == NULL){
        printf("ERROR: date is null");
        return;
    }
    int month_day[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if(is_leap_year(date->year)) month_day[1]++;
    switch (mode) {
        case YEAR:
            date->year++;
            break;
        case MONTH:
            if(date->month == 11){
                date->month = 0;
                increment_date(date,YEAR);
            } else {
                date->month++;
            }
            break;
        case WEEKDAY: // increment 7 days
            date->day+=7;
            if(date->day > month_day[date->month]){
                date->day -= month_day[date->month];
                increment_date(date,MONTH);
            }
            break;
        case DAY:
            date->weekday += 1;
            if(date->weekday > 6){
                date->weekday = 0;
            }
            if(date->day + 1 > month_day[date->month]){
                date->day = 1;
                increment_date(date,MONTH);
            } else
                date->day++;
            break;
        case HOUR:
            if(date->hour == 23){
                date->hour = 0;
                increment_date(date,DAY);
            } else {
                date->hour++;
            }
            break;
        case MINUTE:
            if(date->minute == 59){
                date->minute = 0;
                increment_date(date,HOUR);
            } else {
                date->minute++;
            }
            break;
        case SECOND:
            if(date->second == 59){
                date->second = 0;
                increment_date(date,MINUTE);
            } else {
                date->second++;
            }
            break;
        default:
            printf("ERROR: unexpected arg increment mode");
            return;
    }
}
struct mi_date date_from_time_t(time_t t) {
    struct mi_date d;
    struct tm tm_time;
    localtime_r(&t, &tm_time);
    d.weekday = tm_time.tm_wday;
    d.year = tm_time.tm_year + 1900;
    d.month = tm_time.tm_mon;
    d.day = tm_time.tm_mday;
    d.hour = tm_time.tm_hour;
    d.minute = tm_time.tm_min;
    d.second = tm_time.tm_sec;

    return d;
}
struct mi_date get_next_date(struct mi_date date,struct mi_date template,struct cycle_flags cf){
    while(1) {
        if(date.year > MAX_YEAR){
            date.year = -1;
            return date;
        }
        if (!cf.year) {
            if (date.year != template.year) {
                //no next date
                date.year = -1;
                return date;
            }
        }
        if (!cf.month) {
            if (date.month != template.month) {
                increment_date(&date, MONTH);
                date.minute = 0;
                date.hour = 0;
                date.day = 1;
                date.weekday = calculate_weekday(date);
                continue;
            }
        }
        if (!cf.weekday) {
            if (date.weekday != template.weekday) {
                increment_date(&date, DAY);
                date.minute = 0;
                date.hour = 0;
                continue;
            }
        }
        if (!cf.day) {
            if (date.day != template.day) {
                increment_date(&date, DAY);
                date.minute = 0;
                date.hour = 0;
                continue;
            }
        }
        if (!cf.hour) {
            if (date.hour != template.hour) {
                increment_date(&date, HOUR);
                date.minute = 0;
                continue;
            }
        }
        if (!cf.minute) {
            if (date.minute != template.minute) {
                increment_date(&date, MINUTE);
                date.second = 0;
                continue;
            }
        }
        if (!cf.second) {
            if (date.second != template.second) {
                increment_date(&date, SECOND);
                continue;
            }
        }
        return date;
    }
}