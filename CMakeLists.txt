cmake_minimum_required(VERSION 3.14)
project(rsync)

set(CMAKE_CXX_STANDARD 14)

add_executable(rsync main.cpp rsync.cpp rsync.h md5.cpp md5.h)