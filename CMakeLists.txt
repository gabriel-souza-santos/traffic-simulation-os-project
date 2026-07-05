cmake_minimum_required(VERSION 3.20)

project(urban_traffic LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB SOURCE_FILES CONFIGURE_DEPENDS "src/*.c")
add_executable(urban_traffic ${SOURCE_FILES})

target_include_directories(urban_traffic PRIVATE include)

find_package(Threads REQUIRED)
target_link_libraries(urban_traffic PRIVATE Threads::Threads)

if(MSVC)
    target_compile_options(urban_traffic PRIVATE /W4)
else()
    target_compile_options(urban_traffic PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

set_target_properties(urban_traffic PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)