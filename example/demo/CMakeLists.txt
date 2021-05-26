cmake_minimum_required(VERSION 3.5)
set(CMAKE_BUILD_TYPE Debug)
project(hos_write_demo)

SET(CMAKE_BUILD_TYPE Debug)
link_directories(/opt/MESA/lib/)
include_directories(/opt/MESA/include)

add_executable(hos_write_demo hos_write_demo.cpp)
target_link_libraries(hos_write_demo hos-client-cpp)

