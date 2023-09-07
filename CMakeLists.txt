cmake_minimum_required(VERSION 3.12)
project(cotask)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wno-deprecated -Wall -ggdb -fsanitize=address -fno-omit-frame-pointer -fstack-protector-all")
add_executable(cotask main.cpp task.cpp scheduler.cpp)
target_link_libraries(cotask event fmt)
