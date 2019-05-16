cmake_minimum_required(VERSION 3.12)
project(m3u8 C)

set(CMAKE_C_STANDARD 11)

#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(m3u8 m3u8.c m3u8.h m3u8-hash.h m3u8-types.h m3u8-mastertypes.h m3u8-mediatypes.h)
add_executable(hasher hasher.c)
set_target_properties( m3u8
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        )

configure_file(m3u8.h include/m3u8.h COPYONLY)
configure_file(m3u8-types.h include/m3u8-types.h COPYONLY)
configure_file(m3u8-mediatypes.h include/m3u8-mediatypes.h COPYONLY)
configure_file(m3u8-mastertypes.h include/m3u8-mastertypes.h COPYONLY)
configure_file(LICENSE LICENSE COPYONLY)

#file(COPY ${CMAKE_BUILD_DIR}m3u8.lib DESTINATION ${CMAKE_BUILD_DIR}/lib)