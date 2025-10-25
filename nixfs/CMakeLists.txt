cmake_minimum_required(VERSION 3.6)
project(nixfs VERSION 0.2.1 LANGUAGES C)

configure_file(
  "${PROJECT_SOURCE_DIR}/include/version.h.in"
  "${PROJECT_BINARY_DIR}/include/version.h"
)

include_directories("${PROJECT_BINARY_DIR}/include")

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

# Find and link OpenSSL
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
target_link_libraries(nixfs ${OPENSSL_LIBRARIES})

install(TARGETS nixfs DESTINATION bin)
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink nixfs \$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/mount.nixfs)")
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink nixfs \$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/mount.fuse.nixfs)")
