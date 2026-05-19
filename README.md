# MiCron
POSIX-compliant cron-like task scheduler with client-server IPC implemented in C using POSIX timers,
message queues, threads, and real-time signals.

Łódź University of Technology project 2025/2026

# Instructions
compile program with
gcc -o micron micron.c micron_toolbox_timers.c micron_toolbox_query.c micron_toolbox_date.c logger.c -lpthread -lrt


## scheddule absolute timer task
./micron 0 wd Y M D h m s filename (optional)[launch program args]

wd - week day (0-6)
Y - year, supported up to 2200  
M - month (0-11)  
D - day (1,31)  
h - hour (0,23)  
m - minute(0,59)  
s - seconds(0,59)  

for cyclical execution replace one or more of date elements with '*'

## schedule relative timer task
./micron 1 s m h d w cyclic filename (optional)program args
s - seconds (0-59)  
m - minutes (0-59)  
h - hours (0-23)  
d - days (0-6)  
w - weeks (0-20)  
cyclic - can be 0 or 1, if 1 after launch timer will restart itself until task cancelled

## display scheduled tasks
./micron 2

## cancel task
./micron 3 query_id

## terminate server
./micron 4

## more in-depth overview in readme.txt file
