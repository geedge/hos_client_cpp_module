cmake_minimum_required (VERSION 2.8)

set(lib_name hos-client-cpp)
project(${lib_name})

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
#include(Version)

set(CMAKE_MACOSX_RPATH 0)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

set(CMAKE_INSTALL_PREFIX /opt/MESA)
set(SUPPORT_INSTALL_PREFIX /usr/local/lib64)

add_subdirectory(support) 
add_subdirectory(src) 

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/src/libhos-client-cpp.so DESTINATION ${CMAKE_INSTALL_PREFIX}/lib COMPONENT PROFILE)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/hos_client.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include COMPONENT PROFILE)
#install(FILES ${CMAKE_CURRENT_BINARY_DIR}/src/libhos-client-cpp.a DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT PROFILE)

#include(Package)

