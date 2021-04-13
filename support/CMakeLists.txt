include(ExternalProject)

set(AWSS3_ROOT            ${CMAKE_CURRENT_BINARY_DIR})
set(AWSS3_URL             ${CMAKE_CURRENT_SOURCE_DIR}/aws-sdk-cpp-master.zip)
set(AWSS3_URL_MD5         7afbfc810ad117243f9a794e2d043e9d)
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

