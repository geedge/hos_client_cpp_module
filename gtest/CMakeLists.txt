cmake_minimum_required(VERSION 3.0)
project(gtest_hos_client)
#aux_source_directory(. SRCS)

include_directories("/opt/MESA/include")
link_directories("/opt/MESA/lib")
#link_libraries(hos-client-cpp gtest gtest_main pthread)

add_definitions(-g -W -Wall)
add_executable(gtest_hos_client gtest_hos_init_instance.cpp)
target_link_libraries(gtest_hos_client hos-client-cpp gtest gtest_main pthread)

