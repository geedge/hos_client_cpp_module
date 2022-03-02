cmake_minimum_required (VERSION 2.8)

set(lib_name hos-client-cpp)
project(${lib_name})

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(Version)

set(CMAKE_MACOSX_RPATH 0)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -fno-rtti")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall ")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xlinker --unresolved-symbols=ignore-in-shared-libs")

set(CMAKE_INSTALL_PREFIX /opt/MESA)
set(SUPPORT_INSTALL_PREFIX /usr/local/lib64)

find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
if (CMAKE_CXX_CPPCHECK)
    list(
        APPEND CMAKE_CXX_CPPCHECK 
            "--enable=all"
            "--error-exitcode=0"
            "--suppress=unusedFunction"
            "--suppress=missingInclude"
            "--suppress=uselessAssignmentPtrArg"
            "--suppress=unmatchedSuppression"
    )
    set(CMAKE_C_CPPCHECK ${CMAKE_CXX_CPPCHECK})
else()
  message(FATAL_ERROR "Could not find the program cppcheck.")
endif()

add_subdirectory(support) 
add_subdirectory(src) 

enable_testing()
add_subdirectory(gtest)

add_subdirectory(example/demo)
add_subdirectory(example/performance)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/src/libhos-client-cpp.so DESTINATION ${CMAKE_INSTALL_PREFIX}/lib COMPONENT LIBRARIES)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/hos_client.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include COMPONENT HEADER)
#install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/conf/hos.conf DESTINATION /etc/ld.so.conf.d COMPONENT PROFILE)
#install(FILES ${CMAKE_CURRENT_BINARY_DIR}/src/libhos-client-cpp.a DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT PROFILE)

include(Package)

