cmake_minimum_required(VERSION 3.13)
project(test C)

set(CMAKE_C_STANDARD 99)

add_executable(epoll epoll/main.c)
add_executable(process process/main.c)
add_executable(ipcserver socket-ipc/server.c)
add_executable(ipcclient socket-ipc/client.c)

add_executable(thread thread/main.c)
find_package(Threads REQUIRED)
target_link_libraries(thread Threads::Threads)
