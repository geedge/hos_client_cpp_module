cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_BUILD_TYPE Debug)
project(HosClientPerformance)

link_directories(/usr/local/lib64/)
link_directories(/opt/MESA/lib/)
link_libraries(hos-client-cpp pthread)
include_directories(/opt/MESA/include)

add_executable(HosClientPerformance HosClientPerformance.cpp)
target_link_libraries(HosClientPerformance hos-client-cpp)

