cmake_minimum_required(VERSION 3.14)
project(rsync)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")

link_directories(/usr/local/lib/)
link_libraries(cryptopp)
include_directories(/usr/local/include/)
add_executable(rsync main.cpp rsync.cpp rsync.h)