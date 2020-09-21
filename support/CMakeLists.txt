include(ExternalProject)

set(AWSS3_ROOT            ${CMAKE_CURRENT_BINARY_DIR})
set(AWSS3_URL             ${CMAKE_CURRENT_SOURCE_DIR}/aws-sdk-cpp-master.zip)
set(AWSS3_URL_MD5         a8416a80b15f573e7ac790ca354c8c71)
set(AWSS3_CONFIGURE       cd ${AWSS3_ROOT}/aws-sdk-cpp-master/ && mkdir build)
set(AWSS3_MAKE            cd ${AWSS3_ROOT}/aws-sdk-cpp-master/build && cmake .. -DBUILD_ONLY="s3" && make) 
set(AWSS3_INSTALL         cd ${AWSS3_ROOT}/aws-sdk-cpp-master/build && make install PREFIX=${SUPPORT_INSTALL_PREFIX})

ExternalProject_Add(luajit2
        PREFIX              luajit2
        URL                 ${AWSS3_URL}
        URL_MD5             ${AWSS3_URL_MD5}
        CONFIGURE_COMMAND   ${AWSS3_CONFIGURE}
        CONFIGURE_COMMAND   ""
        BUILD_COMMAND       ${AWSS3_MAKE}
        INSTALL_COMMAND     ${AWSS3_INSTALL}
        BUILD_IN_SOURCE     1
        )


add_library(luajit2-static STATIC IMPORTED GLOBAL)
add_dependencies(luajit2-static luajit2)
set_property(TARGET luajit2-static PROPERTY IMPORTED_LOCATION ${AWSS3_ROOT}/install/lib/libluajit-5.1.a)

