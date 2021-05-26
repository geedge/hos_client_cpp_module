cmake_minimum_required(VERSION 3.5)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -shared -fPIC -std=c++11")

include_directories(${CMAKE_INSTALL_PREFIX}/include/MESA)
link_directories(${CMAKE_INSTALL_PREFIX}/lib)

option(HOS_MOCK "If enabled, the SDK will be built using a MOCK .cpp file for S3." OFF)

file(GLOB HOS_HEADERS "*.h")
file(GLOB HOS_SOURCE "*.cpp")
if (HOS_MOCK)
    add_definitions(-DHOS_MOCK)
    file(GLOB HOS_MOCK_HEADERS "mock/hos_mock.h")
    file(GLOB HOS_MOCK_SOURCE "mock/hos_mock.cpp")
endif()

file(GLOB HOS_SRC
    ${HOS_SOURCE}
    ${HOS_HEADERS}
    ${HOS_MOCK_SOURCE}
    ${HOS_MOCK_HEADERS}
)

add_library(${lib_name}_shared SHARED ${HOS_SRC})
target_link_libraries(${lib_name}_shared
    "-Wl,--whole-archive"
    libaws-c-common.a
    libaws-checksums.a
    libaws-c-event-stream.a
    libaws-cpp-sdk-core.a 
    libaws-cpp-sdk-s3.a
    libtesting-resources.a
    "-Wl,--no-whole-archive"
    libcurl.so
    libpthread.so
    libMESA_prof_load.so
    libMESA_field_stat2.so
    libMESA_handle_logger.so)
set_target_properties(${lib_name}_shared PROPERTIES OUTPUT_NAME ${lib_name})

#for ASAN
set(ASAN_OPTION "OFF" CACHE STRING " set asan type chosen by the user, using OFF as default")
set_property(CACHE ASAN_OPTION  PROPERTY STRINGS OFF ADDRESS THREAD)
message(STATUS "ASAN_OPTION='${ASAN_OPTION}'")

if(ASAN_OPTION MATCHES "ADDRESS")
    set(CMAKE_C_FLAGS "${CMAKADDRESS} -g -DCMAKE_BUILD_TYPE=Debug -fsanitize=address -fno-omit-frame-pointer")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DCMAKE_BUILD_TYPE=Debug -fsanitize=address -fno-omit-frame-pointer")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lasan")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lasan")
elseif(ASAN_OPTION MATCHES "THREAD")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -DCMAKE_BUILD_TYPE=Debug -fsanitize=thread -fno-omit-frame-pointer")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DCMAKE_BUILD_TYPE=Debug -fsanitize=thread -fno-omit-frame-pointer")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lasan")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lasan")
endif()
# end of for ASAN

add_dependencies(${lib_name}_shared aws-sdk-cpp-master-static)
install(TARGETS ${lib_name}_shared LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib COMPONENT LIBRARIES)
