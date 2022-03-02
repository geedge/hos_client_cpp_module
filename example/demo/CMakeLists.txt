cmake_minimum_required(VERSION 3.5)
set(CMAKE_BUILD_TYPE Debug)
project(hos_write_demo)

SET(CMAKE_BUILD_TYPE Debug)
link_directories(/opt/MESA/lib/)
include_directories(/opt/MESA/include)
include_directories("${CMAKE_SOURCE_DIR}/src/")
link_directories("${CMAKE_BINARY_DIR}/src")

add_executable(hos_write_demo hos_write_demo.cpp)
add_dependencies(hos_write_demo ${lib_name}_shared)
target_link_libraries(hos_write_demo hos-client-cpp)

