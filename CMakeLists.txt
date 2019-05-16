cmake_minimum_required(VERSION 3.12)
project(m3u8 C)

set(CMAKE_C_STANDARD 11)

#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(m3u8 src/m3u8.c src/parser.c src/hash.c include/internal-types.h include/m3u8.h include/hash.h include/master-types.h include/media-types.h)
add_executable(hasher hasher.c)
set_target_properties( m3u8
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        )

configure_file(include/m3u8.h include/m3u8.h COPYONLY)
configure_file(include/media-types.h include/media-types.h COPYONLY)
configure_file(include/master-types.h include/master-types.h COPYONLY)
configure_file(LICENSE LICENSE COPYONLY)

#file(COPY ${CMAKE_BUILD_DIR}m3u8.lib DESTINATION ${CMAKE_BUILD_DIR}/lib)