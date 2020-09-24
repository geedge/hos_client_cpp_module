find_path(TCMALLOC_INCLUDE_DIR tcmalloc.h ${PROJECT_SOURCE_DIR}/3rd/tcmalloc /usr/include /usr/include/gperftools /usr/local/include /usr/local/include/gperftools )
find_library(TCMALLOC_LIBRARY NAMES tcmalloc PATHS ${PROJECT_SOURCE_DIR}/3rd/tcmalloc /usr/lib /usr/lib/gperftools /usr/local/lib /usr/local/lib/gperftools)

if (TCMALLOC_INCLUDE_DIR AND TCMALLOC_LIBRARY)
    set(tcmalloc_FOUND TRUE)
endif (TCMALLOC_INCLUDE_DIR AND TCMALLOC_LIBRARY)
