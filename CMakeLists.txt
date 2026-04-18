cmake_minimum_required(VERSION 3.14)
project(diku_parser C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    memento
    GIT_REPOSITORY https://github.com/StraylightRunMOO/memento.git
    GIT_TAG master
)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(memento)

# Interface library (header-only consumption)
add_library(diku_parser INTERFACE)
target_include_directories(diku_parser INTERFACE
    include ${memento_SOURCE_DIR}/include)
target_link_libraries(diku_parser INTERFACE m memento)
target_compile_features(diku_parser INTERFACE c_std_11)

# Single impl TU shared by all executables
add_library(diku_parser_impl STATIC src/impl/diku_parser_impl.c)
target_link_libraries(diku_parser_impl PUBLIC diku_parser)

# CLI tool
add_executable(diku_tool src/test_parser.c)
target_link_libraries(diku_tool PRIVATE diku_parser_impl)

# Legacy alias for the old executable name
set_target_properties(diku_tool PROPERTIES OUTPUT_NAME "test_parser")

# Interactive MUD client
add_executable(mud_client src/mud_client.c)
target_link_libraries(mud_client PRIVATE diku_parser_impl)

# Comprehensive parser test
add_executable(test_parser_comprehensive tests/test_parser_comprehensive.c)
target_link_libraries(test_parser_comprehensive PRIVATE diku_parser_impl)
