cmake_minimum_required(VERSION 3.5)
set(CMAKE_BUILD_TYPE Debug)
project(singleThread)

SET(CMAKE_BUILD_TYPE Debug)
link_directories(/usr/local/lib64/)
link_directories(/opt/MESA/lib/)
link_libraries(hos-client-cpp)
include_directories(/opt/MESA/include)

add_executable(singleThread single_thread.cpp)
target_link_libraries(singleThread hos-client-cpp)

