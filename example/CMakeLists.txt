cmake_minimum_required(VERSION 3.5)
set(CMAKE_BUILD_TYPE Debug)
project(singleThread)

SET(CMAKE_BUILD_TYPE Debug)
link_directories(/usr/local/lib64/)
link_directories(/opt/MESA/lib/)
link_libraries(hos_client_cpp)
#set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})
add_executable(singleThread single_thread.cpp)
target_link_libraries(singleThread hos_client_cpp)

#install(TARGETS singleThread DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
