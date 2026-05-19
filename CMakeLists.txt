cmake_minimum_required(VERSION 3.20)
project(micron C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_executable(micron micron_toolbox_date.c micron_toolbox_date.h micron_toolbox_query.c micron_toolbox_query.h micron_toolbox_timers.c micron_toolbox_timers.h micron.c logger.h logger.c)
target_link_libraries(micron -pthread rt)