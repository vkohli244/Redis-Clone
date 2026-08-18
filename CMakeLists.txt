cmake_minimum_required(VERSION 3.20)

project(redis-starter-cpp LANGUAGES CXX)

include(CTest)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REDIS_CORE_SOURCES
    src/database.cpp
    src/command_handler.cpp
    src/connection.cpp
    src/server.cpp
    src/parser.cpp
)

add_library(redis_core STATIC ${REDIS_CORE_SOURCES})

target_include_directories(redis_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(redis_core PRIVATE
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

add_executable(redis src/main.cpp)
target_link_libraries(redis PRIVATE redis_core)
target_compile_options(redis PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
)
target_link_options(redis PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

if(BUILD_TESTING)
    find_package(Threads REQUIRED)

    add_executable(redis_tests
        tests/test_main.cpp
        tests/parser_test.cpp
        tests/database_test.cpp
        tests/command_handler_test.cpp
        tests/connection_test.cpp
        tests/server_stress_test.cpp
    )

    target_link_libraries(redis_tests PRIVATE redis_core Threads::Threads)
    target_compile_options(redis_tests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    )
    target_link_options(redis_tests PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )

    add_test(NAME redis_tests COMMAND redis_tests)
    set_tests_properties(redis_tests PROPERTIES TIMEOUT 45)
endif()

option(REDIS_ENABLE_COVERAGE "Instrument redis_core and tests for source coverage" OFF)

if(REDIS_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "REDIS_ENABLE_COVERAGE currently requires Clang")
    endif()

    if(NOT BUILD_TESTING)
        message(FATAL_ERROR "REDIS_ENABLE_COVERAGE requires BUILD_TESTING=ON")
    endif()

    target_compile_options(redis_core PRIVATE -O0 -g -fprofile-instr-generate -fcoverage-mapping)
    target_compile_options(redis_tests PRIVATE -O0 -g -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(redis_tests PRIVATE -fprofile-instr-generate -fcoverage-mapping)

    find_program(LLVM_PROFDATA NAMES llvm-profdata)
    find_program(LLVM_COV NAMES llvm-cov)

    if(APPLE AND (NOT LLVM_PROFDATA OR NOT LLVM_COV))
        find_program(XCRUN NAMES xcrun REQUIRED)
        execute_process(
            COMMAND ${XCRUN} --find llvm-profdata
            OUTPUT_VARIABLE LLVM_PROFDATA
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY
        )
        execute_process(
            COMMAND ${XCRUN} --find llvm-cov
            OUTPUT_VARIABLE LLVM_COV
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY
        )
    endif()

    if(NOT LLVM_PROFDATA OR NOT LLVM_COV)
        message(FATAL_ERROR "llvm-profdata and llvm-cov are required for coverage")
    endif()

    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_BINARY_DIR}/redis-tests.profraw
        COMMAND ${CMAKE_COMMAND} -E env
                LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/redis-tests.profraw
                $<TARGET_FILE:redis_tests>
        COMMAND ${LLVM_PROFDATA} merge -sparse
                ${CMAKE_BINARY_DIR}/redis-tests.profraw
                -o ${CMAKE_BINARY_DIR}/redis-tests.profdata
        COMMAND ${LLVM_COV} report $<TARGET_FILE:redis_tests>
                -instr-profile=${CMAKE_BINARY_DIR}/redis-tests.profdata
                -ignore-filename-regex=.*/tests/.*
                -show-region-summary=false
                -show-branch-summary=false
        DEPENDS redis_tests
        USES_TERMINAL
        VERBATIM
    )
endif()
