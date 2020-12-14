# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

if("/home/pxz/project/hos_client_cpp_module/support/aws-sdk-cpp-master.zip" STREQUAL "")
  message(FATAL_ERROR "LOCAL can't be empty")
endif()

if(NOT EXISTS "/home/pxz/project/hos_client_cpp_module/support/aws-sdk-cpp-master.zip")
  message(FATAL_ERROR "File not found: /home/pxz/project/hos_client_cpp_module/support/aws-sdk-cpp-master.zip")
endif()

if("MD5" STREQUAL "")
  message(WARNING "File will not be verified since no URL_HASH specified")
  return()
endif()

if("02e237ee4879ee96ab74e750816cc777" STREQUAL "")
  message(FATAL_ERROR "EXPECT_VALUE can't be empty")
endif()

message(STATUS "verifying file...
     file='/home/pxz/project/hos_client_cpp_module/support/aws-sdk-cpp-master.zip'")

file("MD5" "/home/pxz/project/hos_client_cpp_module/support/aws-sdk-cpp-master.zip" actual_value)

if(NOT "${actual_value}" STREQUAL "02e237ee4879ee96ab74e750816cc777")
  message(FATAL_ERROR "error: MD5 hash of
  /home/pxz/project/hos_client_cpp_module/support/aws-sdk-cpp-master.zip
does not match expected value
  expected: '02e237ee4879ee96ab74e750816cc777'
    actual: '${actual_value}'
")
endif()

message(STATUS "verifying file... done")
