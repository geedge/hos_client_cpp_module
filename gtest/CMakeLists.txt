cmake_minimum_required(VERSION 3.0)
project(gtest_hos_client)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRCS)

include_directories("/opt/MESA/include")
link_directories("/opt/MESA/lib")
#link_libraries(hos-client-cpp gtest gtest_main pthread)

# coverage
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")

add_definitions(-g -W -Wall -std=c++11)
#add_executable(gtest_hos_client gtest_hos_init_instance.cpp gtest_hos_get_instance.cpp gtest_hos_close_fd.cpp gtest_hos_open_fd.cpp)
#add_executable(gtest_hos_client CheckHosClient.cpp gtest_hos_upload_buff.cpp)
add_executable(gtest_hos_client ${SRCS})
target_link_libraries(gtest_hos_client hos-client-cpp gtest gtest_main pthread)

