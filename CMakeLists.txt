cmake_minimum_required(VERSION 4.1)

project(urban_traffic LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB INCLUDE_FILES "include/*.h")
file(GLOB SOURCE_FILES "src/*.c")

add_executable(urban_traffic ${INCLUDE_FILES} ${SOURCE_FILES}
        include/debug.h
        src/vehicle.c)

target_include_directories(urban_traffic PRIVATE include)