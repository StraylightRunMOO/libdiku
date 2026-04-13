cmake_minimum_required(VERSION 3.10)
project(diku_parser C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Library
add_library(diku_parser STATIC src/diku_parser.c)
target_include_directories(diku_parser PUBLIC include)
target_link_libraries(diku_parser PUBLIC m)

# CLI tool
add_executable(diku_tool src/test_parser.c)
target_link_libraries(diku_tool PRIVATE diku_parser)

# Legacy alias for the old executable name
set_target_properties(diku_tool PROPERTIES OUTPUT_NAME "test_parser")

# Interactive MUD client
add_executable(mud_client src/mud_client.c)
target_link_libraries(mud_client PRIVATE diku_parser)
