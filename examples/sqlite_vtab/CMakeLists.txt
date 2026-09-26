# SQLite virtual-table extension for diku parser.
# Enable by configuring the top-level project with -DDIKU_BUILD_SQLITE=ON.

# Static library the test binary and other C hosts link against.
add_library(diku_sqlite_static STATIC diku_sqlite.c)
target_include_directories(diku_sqlite_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SQLite3_INCLUDE_DIRS})
target_link_libraries(diku_sqlite_static PUBLIC diku_impl SQLite::SQLite3)
target_compile_definitions(diku_sqlite_static PUBLIC DIKU_SQLITE_EMBED)

# Loadable extension (.so) usable via `.load` from the sqlite3 CLI.
add_library(diku_sqlite MODULE diku_sqlite.c)
target_include_directories(diku_sqlite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SQLite3_INCLUDE_DIRS})
target_link_libraries(diku_sqlite PRIVATE diku_impl SQLite::SQLite3)
set_target_properties(diku_sqlite PROPERTIES
    PREFIX ""
    OUTPUT_NAME "diku_sqlite")

# Test binary — mirrors tests/test_parser_comprehensive.c pattern.
add_executable(test_diku_sqlite test_diku_sqlite.c)
target_link_libraries(test_diku_sqlite PRIVATE diku_sqlite_static)

add_test(NAME diku_sqlite
    COMMAND test_diku_sqlite
        ${CMAKE_SOURCE_DIR}/data
        ${CMAKE_SOURCE_DIR}/data2)
set_tests_properties(diku_sqlite PROPERTIES TIMEOUT 300)

# Convenience target: copy the built .so into ~/.local/lib and drop a
# ~/.sqliterc snippet that .load's it silently. Run once with
#   make install_diku_sqlite_user
# and every subsequent sqlite3 invocation has diku_load available.
set(DIKU_SQLITE_USER_LIB "$ENV{HOME}/.local/lib/diku_sqlite.so")
set(DIKU_SQLITERC "$ENV{HOME}/.sqliterc")
add_custom_target(install_diku_sqlite_user
    COMMAND ${CMAKE_COMMAND} -E make_directory $ENV{HOME}/.local/lib
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:diku_sqlite>
        ${DIKU_SQLITE_USER_LIB}
    COMMAND ${CMAKE_COMMAND}
        -DSQLITERC=${DIKU_SQLITERC}
        -DEXT_PATH=$ENV{HOME}/.local/lib/diku_sqlite
        -DEXT_SO=${DIKU_SQLITE_USER_LIB}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/write_sqliterc.cmake
    DEPENDS diku_sqlite
    COMMENT "Installing diku_sqlite.so -> ${DIKU_SQLITE_USER_LIB} + updating ${DIKU_SQLITERC}")
