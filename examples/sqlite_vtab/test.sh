#!/usr/bin/env bash
# Test the loadable extension via the sqlite3 CLI.
#
# Usage:
#   ./examples/sqlite_vtab/test.sh [path-to-diku_sqlite.so] [path-to-are-file]
#
# Defaults assume an out-of-source build in ./build.

set -euo pipefail

EXT="${1:-build/examples/sqlite_vtab/diku_sqlite}"
ARE="${2:-data/dikumud/pagesvisited.are}"

if [ ! -f "${EXT}.so" ] && [ ! -f "${EXT}" ]; then
    echo "extension not found at ${EXT}" >&2
    exit 1
fi
if [ ! -e "${ARE}" ]; then
    echo "area file not found at ${ARE}" >&2
    exit 1
fi

echo "== .load ${EXT}"
echo "== diku_load('${ARE}', 't')"

sqlite3 :memory: <<SQL
.load ${EXT}
SELECT diku_load('${ARE}', 't') AS areas_loaded;
SELECT 'rooms:' AS t, COUNT(*) FROM t_rooms;
SELECT 'mobiles:' AS t, COUNT(*) FROM t_mobiles;
SELECT 'items:' AS t, COUNT(*) FROM t_items;
SELECT 'exits:' AS t, COUNT(*) FROM t_exits;
SELECT vnum, name FROM t_rooms ORDER BY vnum LIMIT 3;
SELECT direction_name, from_vnum, to_vnum FROM t_exits ORDER BY from_vnum LIMIT 5;
SELECT diku_unload('t');
SQL
