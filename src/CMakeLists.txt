cmake_minimum_required(VERSION 3.5)

#SET(lib_name hos_client_cpp)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -shared -fPIC -std=c++11")
SET(CMAKE_BUILD_TYPE Debug)

include_directories(${CMAKE_INSTALL_PREFIX}/include/MESA)
link_directories(${SUPPORT_INSTALL_PREFIX})
link_directories(${CMAKE_INSTALL_PREFIX}/lib)
set(CMKAE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -shared -fPIC")
add_library(${lib_name}_shared SHARED hos_client.cpp hos_hash.cpp)
target_link_libraries(${lib_name}_shared libaws-cpp-sdk-s3.so libaws-cpp-sdk-core.so libMESA_field_stat2.so libcurl.so)
set_target_properties(${lib_name}_shared PROPERTIES OUTPUT_NAME ${lib_name})

add_dependencies(${lib_name}_shared aws-sdk-cpp-master-static)
#install(TARGETS ${lib_name}_shared LIBRARY DESTINATION /opt/MESA/lib)

install(TARGETS ${lib_name}_shared LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib COMPONENT LIBRARIES)
