cmake_minimum_required(VERSION 3.0)
project(nixfs C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(include)

file(GLOB_RECURSE SOURCES LIST_DIRECTORIES false src/*.c)
add_executable(nixfs ${SOURCES})

# Add the -D_FILE_OFFSET_BITS=64 compile flag
set_property(TARGET nixfs PROPERTY COMPILE_DEFINITIONS _FILE_OFFSET_BITS=64)

# Find and link the Fuse library
find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE REQUIRED fuse)
include_directories(${FUSE_INCLUDE_DIRS})
target_link_libraries(nixfs ${FUSE_LIBRARIES})

install(TARGETS nixfs DESTINATION bin)
