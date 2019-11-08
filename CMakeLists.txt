cmake_minimum_required(VERSION 3.14)
project(rsync)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")

include_directories("deps")

find_package(OpenSSL REQUIRED)

if( OPENSSL_FOUND )
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    include_directories(${OPENSSL_INCLUDE_DIR})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
else()
    # Error; with REQUIRED, pkg_search_module() will throw an error by it's own
endif()

add_executable(rsync main.cpp rsync.cpp rsync.h)
target_link_libraries(rsync OpenSSL::Crypto)

add_executable(jsonresult makejsonresult.cpp rsync.cpp rsync.h)
target_link_libraries(jsonresult OpenSSL::Crypto)

add_executable(div div.cpp)