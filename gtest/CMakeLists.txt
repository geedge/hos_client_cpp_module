cmake_minimum_required(VERSION 3.0)
project(gtest_hos_client)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRCS)

include_directories("/opt/MESA/include")
include_directories("/opt/MESA/include/MESA")
include_directories("${CMAKE_BINARY_DIR}/support/GoogleTest/include/")
link_directories("/opt/MESA/lib")
link_directories("${CMAKE_BINARY_DIR}/src/")
link_directories("${CMAKE_BINARY_DIR}/support/GoogleTest/lib/")
#link_libraries(hos-client-cpp gtest gtest_main pthread)

# coverage
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage -fPIE")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage -fPIE")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage -fPIE")

add_definitions(-g -W -Wall -std=c++11 -fPIE)
#add_executable(gtest_hos_client gtest_hos_init_instance.cpp gtest_hos_get_instance.cpp gtest_hos_close_fd.cpp gtest_hos_open_fd.cpp)
#add_executable(gtest_hos_client CheckHosClient.cpp gtest_hos_init_instance.cpp)
add_executable(gtest_hos_client gtest_hos_write.cpp CheckHosClient.cpp)
# add_executable(gtest_hos_client ${SRCS})
add_dependencies(gtest_hos_client ${lib_name}_shared gtest)
target_link_libraries(gtest_hos_client hos-client-cpp gtest gtest_main pthread)

add_test(NAME SOFT_LINK COMMAND sh -c "ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/conf ${CMAKE_BINARY_DIR}/conf")
add_test(NAME MKDIR COMMAND sh -c "cd ${CMAKE_CURRENT_BINARY_DIR} && mkdir log")
add_test(NAME HOS_TEST COMMAND gtest_hos_client)

