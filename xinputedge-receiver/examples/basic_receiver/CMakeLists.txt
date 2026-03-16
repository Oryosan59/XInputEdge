cmake_minimum_required(VERSION 3.16)
project(basic_receiver C)

add_executable(basic_receiver main.c)
find_package(Threads REQUIRED)
target_link_libraries(basic_receiver PRIVATE Threads::Threads)
target_link_libraries(basic_receiver PUBLIC xinputedge)
