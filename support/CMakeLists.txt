include(ExternalProject)

set(AWSS3_ROOT            ${CMAKE_CURRENT_BINARY_DIR})
set(AWSS3_URL             ${CMAKE_CURRENT_SOURCE_DIR}/aws-sdk-cpp-master.zip)
set(AWSS3_URL_MD5         a38984c137d7768dec2a7cde02874c32)
set(AWSS3_CONFIGURE       cd ${AWSS3_ROOT}/aws-sdk-cpp-master/src/aws-sdk-cpp-master && cmake . -DBUILD_ONLY=s3)
set(AWSS3_MAKE            cd ${AWSS3_ROOT}/aws-sdk-cpp-master/src/aws-sdk-cpp-master && make) 
set(AWSS3_INSTALL         cd ${AWSS3_ROOT}/aws-sdk-cpp-master/src/aws-sdk-cpp-master && make install PREFIX=${SUPPORT_INSTALL_PREFIX})

ExternalProject_Add(aws-sdk-cpp-master
        PREFIX              aws-sdk-cpp-master
        URL                 ${AWSS3_URL}
        URL_MD5             ${AWSS3_URL_MD5}
        CONFIGURE_COMMAND   ${AWSS3_CONFIGURE}
        BUILD_COMMAND       ${AWSS3_MAKE}
        INSTALL_COMMAND     ${AWSS3_INSTALL}
        BUILD_IN_SOURCE     1
        )


    add_library(aws-sdk-cpp-master-static STATIC IMPORTED GLOBAL)
    add_dependencies(aws-sdk-cpp-master-static aws-sdk-cpp-master)
    set_property(TARGET aws-sdk-cpp-master-static PROPERTY IMPORTED_LOCATION ${SUPPORT_INSTALL_PREFIX})

#### GoogleTest
ExternalProject_Add(GoogleTest 
        PREFIX GoogleTest
        URL ${CMAKE_CURRENT_SOURCE_DIR}/googletest-release-1.8.0.tar.gz
        URL_MD5 16877098823401d1bf2ed7891d7dce36
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DBUILD_SHARED_LIBS=ON)

ExternalProject_Get_Property(GoogleTest INSTALL_DIR)
file(MAKE_DIRECTORY ${INSTALL_DIR}/include)
add_library(gtest SHARED IMPORTED GLOBAL)
add_dependencies(gtest GoogleTest)
set_property(TARGET gtest PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/libgtest.so)
#set_property(TARGET gtest PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/libgtest_main.so)
set_property(TARGET gtest PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include)
set_property(TARGET gtest PROPERTY INTERFACE_LINK_LIBRARIES pthread)


