cmake_minimum_required(VERSION 3.5)
set(CMAKE_BUILD_TYPE Debug)
project(hos_write_complete)
project(hos_upload_complete)

SET(CMAKE_BUILD_TYPE Debug)
link_directories(/usr/local/lib64/)
link_directories(/opt/MESA/lib/)
link_libraries(hos-client-cpp)
include_directories(/opt/MESA/include)

add_executable(hos_write_complete hos_write_complete.cpp)
add_executable(hos_upload_complete hos_upload_complete.cpp)
target_link_libraries(hos_write_complete hos-client-cpp)
target_link_libraries(hos_upload_complete hos-client-cpp)

