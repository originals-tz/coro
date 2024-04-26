SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_EXTENSIONS OFF)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置宏与编译参数
set(CMAKE_CXX_FLAGS "-Wno-deprecated -Wall -ggdb")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")

set(CMAKE_C_FLAGS "-Wno-deprecated -Wall -ggdb")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-O2 -g")

include_directories("..")
link_directories("/usr/lib/aarch64-linux-gnu"
        "/usr/lib64/mysql")

file(GLOB_RECURSE CPP_SRC_LIST ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
foreach (v ${CPP_SRC_LIST})
    string(REGEX MATCH "test/.*" relative_path ${v})
    string(REGEX REPLACE "test/" "" target_name ${relative_path})
    string(REGEX REPLACE ".cpp" "" target_name ${target_name})
    add_executable(${target_name}_test ${v} ${ECACHE_SOURCE})
    target_link_libraries(${target_name}_test
            event
            pthread
            gtest
            gtest_main
            mysqlclient
    )
    target_compile_options(${target_name}_test PRIVATE -fsanitize=address -fno-omit-frame-pointer -fstack-protector-all)
    target_link_options(${target_name}_test PRIVATE -fsanitize=address)
    add_test(NAME ${target_name}_test COMMAND ${target_name}_test
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endforeach ()
