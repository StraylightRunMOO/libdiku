# diku_sqlite — SQLite virtual-table extension

A loadable SQLite extension that exposes DikuMUD area files as virtual
tables so they can be queried and joined from SQL.

## Build

```bash
mkdir build && cd build
cmake -DDIKU_BUILD_SQLITE=ON ..
make -j
```

Produces:

- `examples/sqlite_vtab/diku_sqlite.so` — loadable via the `sqlite3` CLI's
  `.load` command.
- `examples/sqlite_vtab/libdiku_sqlite_static.a` — link this into your
  own C host and call `diku_sqlite_register(sqlite3*)`.
- `examples/sqlite_vtab/test_diku_sqlite` — the stress test.

## Use from the `sqlite3` CLI

### One-time auto-load setup

```
make install_diku_sqlite_user
```

This copies the `.so` to `~/.local/lib/diku_sqlite.so` and adds a
silent-tolerant `.load` block to `~/.sqliterc`. Any future bare
`sqlite3` invocation has `diku_load` available with no `.load` needed.
The block is marker-fenced so re-running the target updates in place
without duplicating.

### Manual load

```
$ sqlite3
sqlite> .load ./examples/sqlite_vtab/diku_sqlite
sqlite> SELECT diku_load('data/dikumud/pagesvisited.are', 'p');
sqlite> SELECT vnum, name FROM p_rooms LIMIT 5;
sqlite> SELECT r.name, e.direction_name, t.name
        FROM p_exits e
        JOIN p_rooms r ON r.area_id = e.area_id AND r.vnum = e.from_vnum
        LEFT JOIN p_rooms t ON t.area_id = e.area_id AND t.vnum = e.to_vnum
        LIMIT 20;
sqlite> SELECT diku_unload('p');
```

`diku_load(path, prefix)` accepts:

- A single `.are` file.
- A CircleMUD package base path (`data/circle/3` → `3.wld/.mob/.obj/.zon`).
- A folder — recursively picks up `.are` files and Circle packages.

## SQL surface

Scalar functions:

- `diku_load(path, prefix)` — load, returns the number of areas parsed.
- `diku_unload(prefix)` — drop tables and free the areas. Returns 1 if a
  prefix was removed, 0 otherwise.
- `diku_reapply_resets(prefix)` — re-run reset parsing after edits.
- `diku_dir_name(int)`, `diku_sector_name(int)`, `diku_item_type_name(int)`,
  `diku_format_name(int)` — enum → text helpers.

Per-prefix tables (columns match `include/diku/types.h` field names where
they're scalar; nested arrays get their own child tables):

- `<prefix>_areas` — one row per loaded area.
- `<prefix>_rooms` — one row per room.
- `<prefix>_mobiles`, `<prefix>_items` — self-explanatory.
- `<prefix>_exits` — one row per non-null `room->exits[d]`.
- `<prefix>_resets` — one row per raw reset line, with the leading
  command letter and up to 5 numeric args parsed out.
- `<prefix>_extra_descs` — child table over rooms and items
  (distinguished by `owner_kind`).
- `<prefix>_affects` — child table over item affects.
- `<prefix>_room_mobiles`, `<prefix>_room_items` — populated by reset
  parsing; the mobiles/items placed in each room.

## Writes are in-memory only

`UPDATE` is supported on all scalar columns of `rooms`, `mobiles`,
`items`, and `areas`. `INSERT` is supported on `rooms`, `mobiles`, and
`items`. `DELETE` is a *soft delete* on those three tables — the row's
`vnum` is flipped to `INT_MIN` and skipped during iteration.

Writes are NOT persisted back to `.are` files. The library ships no
serializer; the extension mutates the live `area_t` in memory so that
subsequent `SELECT`s and C-API calls see the change, and that's it.

`sqlite3_changes()` after a DELETE reports how many rows were flagged
soft-deleted.

## Embedding from C

```c
#include <sqlite3.h>
#include "diku_sqlite.h"

sqlite3 *db;
sqlite3_open(":memory:", &db);
diku_sqlite_register(db);

sqlite3_exec(db, "SELECT diku_load('data/haon.are', 'h')", NULL, NULL, NULL);
/* ... query h_rooms, h_exits, etc. ... */
sqlite3_close(db);
```

Link against `diku_sqlite_static` (or `diku_impl` + `diku_sqlite.c`
compiled with `-DDIKU_SQLITE_EMBED`).
