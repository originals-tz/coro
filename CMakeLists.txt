cmake_minimum_required(VERSION 3.12)
project(cotask)

set(CMAKE_CXX_STANDARD 20)

add_executable(cotask main.cpp task.cpp)
target_link_libraries(cotask event)
