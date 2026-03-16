cmake_minimum_required(VERSION 3.16)
project(cli_robot_sim C)

find_package(Threads REQUIRED)

add_executable(cli_robot_sim main.c)
target_link_libraries(cli_robot_sim PRIVATE xinputedge Threads::Threads)
