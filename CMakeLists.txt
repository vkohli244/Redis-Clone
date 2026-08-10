cmake_minimum_required(VERSION 3.13)

project(redis-starter-cpp LANGUAGES CXX)

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
