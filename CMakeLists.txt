cmake_minimum_required(VERSION 3.15)
project(OS1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c++11 -Wall -Wreorder -pedantic-errors -Werror -DNDEBUG")
add_executable(OS1 Commands.cpp Commands.h signals.cpp signals.h smash.cpp)