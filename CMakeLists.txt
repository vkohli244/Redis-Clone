cmake_minimum_required(VERSION 3.20)

project(redis-starter-cpp LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(redis
    src/main.cpp
    src/database.cpp
    src/command_handler.cpp
    src/connection.cpp
    src/server.cpp
    src/parser.cpp
)

target_include_directories(redis PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(redis PRIVATE
    -Wall
    -Wextra
    -Wpedantic

    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Wold-style-cast
    -Wcast-align
    -Wcast-qual
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wnull-dereference
    -Wundef
    -Wswitch-enum
    -Wmissing-declarations

    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
)

target_link_options(redis PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
