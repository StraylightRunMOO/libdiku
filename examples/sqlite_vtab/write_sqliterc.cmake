# Idempotently install a diku-sqlite auto-load block into ~/.sqliterc.
# Preserves any user content outside the marked block.
#
# Inputs (passed via -D on the cmake command line):
#   SQLITERC — target ~/.sqliterc path
#   EXT_PATH — the .load argument (path without .so extension)
#   EXT_SO   — the actual .so file (used in the existence test)

if(NOT DEFINED SQLITERC OR NOT DEFINED EXT_PATH OR NOT DEFINED EXT_SO)
    message(FATAL_ERROR "write_sqliterc.cmake: SQLITERC, EXT_PATH, EXT_SO all required")
endif()

set(BEGIN_MARKER "-- >>> diku sqlite auto-load >>>")
set(END_MARKER   "-- <<< diku sqlite auto-load <<<")
set(LOAD_HELPER  "${EXT_SO}.load")

set(BLOCK
"${BEGIN_MARKER}
-- Regenerate this block with `make install_diku_sqlite_user` in the diku build tree.
-- Silent-tolerant: if the extension .so is missing, no error prints.
.system if [ -f ${EXT_SO} ]; then echo \".load ${EXT_PATH}\" > ${LOAD_HELPER}; else : > ${LOAD_HELPER}; fi
.read ${LOAD_HELPER}
${END_MARKER}")

set(EXISTING "")
if(EXISTS ${SQLITERC})
    file(READ ${SQLITERC} EXISTING)
endif()

# Strip any prior block so re-runs don't accumulate.
string(REGEX REPLACE
    "\n?${BEGIN_MARKER}.*${END_MARKER}\n?"
    "\n"
    STRIPPED
    "${EXISTING}")

# Trim leading/trailing blank lines from stripped content.
string(REGEX REPLACE "^[\n\r]+" "" STRIPPED "${STRIPPED}")
string(REGEX REPLACE "[\n\r]+$" "" STRIPPED "${STRIPPED}")

if(STRIPPED STREQUAL "")
    set(NEW_CONTENT "${BLOCK}\n")
else()
    set(NEW_CONTENT "${STRIPPED}\n\n${BLOCK}\n")
endif()

file(WRITE ${SQLITERC} "${NEW_CONTENT}")
message(STATUS "diku-sqlite auto-load block written to ${SQLITERC}")
