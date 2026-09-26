/*
 * diku_sqlite.h — SQLite virtual-table extension for the DikuMUD area parser.
 *
 * Two entry points:
 *
 *   int diku_sqlite_register(sqlite3 *db);
 *       Register the module and its scalar functions on an existing
 *       connection. Use this when linking the extension statically into a
 *       C host (see the DIKU_SQLITE_EMBED build).
 *
 *   int sqlite3_diku_init(sqlite3 *db, char **err,
 *                         const sqlite3_api_routines *api);
 *       SQLite's loadable-extension entry point (SQLITE_EXTENSION_INIT1).
 *       Called automatically when the sqlite3 CLI runs `.load libdiku_sqlite`.
 *
 * SQL surface after registration:
 *
 *   SELECT diku_load(path, prefix);       -- ingest a file/folder/package
 *   SELECT diku_unload(prefix);           -- drop tables and free areas
 *   SELECT diku_reapply_resets(prefix);   -- re-run reset parsing after edits
 *   SELECT diku_dir_name(0);              -- enum helpers
 *   SELECT diku_sector_name(3);
 *   SELECT diku_item_type_name(5);
 *   SELECT diku_format_name(2);
 *
 * Per-prefix virtual tables:
 *   <prefix>_areas          <prefix>_rooms          <prefix>_mobiles
 *   <prefix>_items          <prefix>_exits          <prefix>_resets
 *   <prefix>_extra_descs    <prefix>_affects
 *   <prefix>_room_mobiles   <prefix>_room_items
 *
 * Writes are in-memory only: UPDATE/INSERT/DELETE mutate the live area_t
 * graph but are not persisted back to .are files (the library has no
 * serializer). DELETE is a soft-delete: the row's vnum is set to INT_MIN
 * and skipped during iteration.
 */

#ifndef DIKU_SQLITE_H
#define DIKU_SQLITE_H

#include <sqlite3.h>

#ifdef __cplusplus
extern "C" {
#endif

int diku_sqlite_register(sqlite3 *db);

int sqlite3_diku_init(sqlite3 *db,
                      char **pzErrMsg,
                      const sqlite3_api_routines *pApi);

/* Test/embedding helper: look up the head of the area_t linked list that
 * a given prefix loaded. Returns NULL if the prefix isn't registered.
 * The returned pointer is owned by the extension and stays valid until
 * diku_unload(prefix) or the connection is closed. */
struct area_t *diku_sqlite_get_areas(sqlite3 *db, const char *prefix);

#ifdef __cplusplus
}
#endif

#endif /* DIKU_SQLITE_H */
