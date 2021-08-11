cmake_minimum_required (VERSION 2.8)

project (HttpServer)

add_compile_options (-std=c++11 -O2 -Wall)
set (SRCS src/http_server.cpp src/worker_pool.cpp src/io.cpp)

add_executable (http_server src/main.cpp ${SRCS})
target_link_libraries (http_server pthread)

# TODO: remove
add_executable (final src/main.cpp ${SRCS})
target_link_libraries (final pthread)
