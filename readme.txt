project summary:

micron is a small cron-like scheduler written in C for Linux/POSIX systems.
The first ./micron process starts a background server and then sends the user's
query to it as a client. Later ./micron runs connect to the same POSIX message
queue and send schedule, display, cancel, or terminate requests to that server.

The server supports two scheduling modes:
- absolute date/time schedules, with '*' accepted for recurring date fields
- relative timers, either single-use or cyclic

Scheduled tasks are stored in memory by the running server. Display output is
written by the server to micron_tasks.txt and then printed by the client.
The project also includes a signal-controlled logger and dump system that can
write runtime logs and timer-state dumps without stopping the server.


compilation:
gcc -o micron micron.c micron_toolbox_timers.c micron_toolbox_query.c micron_toolbox_date.c logger.c -lpthread -lrt

if previous server crashed, run:
./micron cleanup

queries - clients

------------------------------------------------------------------------------------------------------------
currently no server-side query feedback is implemented, and query is validated in client
------------------------------------------------------------------------------------------------------------

./micron 0
mode 0 - standard cron launch

args:
wd Y M D h m s filename (optional)[launch program args]

launches file of filename at specified date, if date is before current time it will not be executed

wd - week day (0-6)
Y - year, supported up to 2200
M - month (0-11)
D - day (1,31)
h - hour (0,23)
m - minute(0,59)
s - seconds(0,59)

sample launch:
./micron 0 0 2026 0 4 18 35 40 /sample_program arg1 arg2 
-> execute sample_program on monday 2026 Jan 4th at 18:35:40 with arg1 and arg2 as launch arguments

for cyclical execution replace one or more of date elements with '*' (asterisks included)
'*' is interpreted as every or any eg:

./micron 0 '*' 2026 0 4 18 '*' 40 /sample_program arg1 arg2
-> execute program on 2026 Jan 4th without considering weekday, every minute of 18th hour, at 18:00:40, 18:01:40 etc.

------------------------------------------------------------------------------------------------------------

./micron 1 - relative timer cron launch

schedule timed task (mode 1)
s m h d w cyclic filename (optional)program args
s - seconds (0-59)
m - minutes (0-59)
h - hours (0-23)
d - days (0-6)
w - weeks (0-20)
min interval - 1s
cyclic - can be 0 or 1, if 1 after launch timer will restart itself until task cancelled

sample launch:
./micron 1 30 1 0 0 0 0 /sample_program
-> launch sample_program.c in 1min 30s 
./micron 1 0 0 1 0 0 1 /sample_program
-> launch sample_program.c every hour

------------------------------------------------------------------------------------------------------------

display scheduled tasks
./micron 2
displays all current tasks, including their id's

sample output

./micron 2
running as client
query processed and sent to server
id: 0	schedule mode: relative	creation date: 2026-05-18 16:06	interval: 0 weeks, 0 days, 0 hours, 1 minutes, 30 seconds. SINGLE-USE 
 task: sample_program
id: 1	schedule mode: relative	creation date: 2026-05-18 16:07	interval: 0 weeks, 0 days, 1 hours, 0 minutes, 0 seconds. CYCLE 
 task: sample_program 
id: 2	schedule mode: relative	creation date: 2026-05-18 16:07	interval: 0 weeks, 0 days, 0 hours, 1 minutes, 30 seconds. SINGLE-USE 
 task: sample_program 
id: 3	schedule mode: absolute	schedule: * 2026 10 7 18 * 40 
 task: /sample_program arg1 arg2

------------------------------------------------------------------------------------------------------------

cancel task
./micron 3 query_id

each task can be cancelled by running ./micron 3 with task's id as argument

------------------------------------------------------------------------------------------------------------

terminate server
./micron 4
server will shut down
all tasks will be canceled

------------------------------------------------------------------------------------------------------------

specs:
max filename 31
max argument len 31
max arguments for file launch 16
max task number for server 50

------------------------------------------------------------------------------------------------------------

logger control:

log filename: micron_logger.txt
send signals to server pid to control logger

logging toggle signal: SIGRTMIN + 1

dump signal: SIGRTMIN
si_value:
1 - HEAP
2 - DATA
3 - STACK
logger is configured so that all dump modes currently use dump_all_timers

verbosity signal: SIGRTMIN + 2
si_value:
1 - MIN
2 - STANDARD
3 - MAX

