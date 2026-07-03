cmake_minimum_required(VERSION 3.14)
project(diku C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    memento
    GIT_REPOSITORY https://github.com/StraylightRunMOO/memento.git
    GIT_TAG master
)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(memento)

FetchContent_Declare(
    libspatial
    GIT_REPOSITORY https://github.com/kevinfling/libspatial.git
    GIT_TAG master
)
set(LIBSPATIAL_USE_MEMENTO OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libspatial)

# Interface library (header-only C consumption)
# Interface library (header-only C consumption)
add_library(diku INTERFACE)
target_include_directories(diku INTERFACE
    include ${memento_SOURCE_DIR}/include ${libspatial_SOURCE_DIR}/include)
target_link_libraries(diku INTERFACE m memento)
target_compile_features(diku INTERFACE c_std_11)
set_target_properties(diku PROPERTIES OUTPUT_NAME "libdiku")

# Single impl TU shared by all executables
add_library(diku_impl STATIC src/impl/diku_parser_impl.c)
target_link_libraries(diku_impl PUBLIC diku)
set_target_properties(diku_impl PROPERTIES OUTPUT_NAME "diku_impl")

# CLI tool
add_executable(diku_tool src/test_parser.c)
target_link_libraries(diku_tool PRIVATE diku_impl)

# Legacy alias for the old executable name
set_target_properties(diku_tool PROPERTIES OUTPUT_NAME "test_parser")

# Interactive MUD client
add_executable(mud_client src/mud_client.c)
target_link_libraries(mud_client PRIVATE diku_impl)

# Comprehensive parser test
add_executable(test_parser_comprehensive tests/test_parser_comprehensive.c)
target_link_libraries(test_parser_comprehensive PRIVATE diku_impl)
