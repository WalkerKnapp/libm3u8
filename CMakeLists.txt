cmake_minimum_required(VERSION 3.12)
project(m3u8 C)

set(CMAKE_C_STANDARD 11)

add_library(m3u8 m3u8.c m3u8.h m3u8-hash.h m3u8-types.h m3u8-mastertypes.h m3u8-mediatypes.h)