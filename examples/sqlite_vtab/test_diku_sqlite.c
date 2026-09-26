/*
 * test_diku_sqlite.c — stress test for the SQLite virtual-table extension.
 *
 * Walks .are files under one or more folders, loads each into an in-memory
 * SQLite database via diku_load(), and asserts that the SQL-visible row
 * counts and lookups agree with the underlying C API.
 *
 * Mirrors the enumerate-and-report pattern used by
 * tests/test_parser_comprehensive.c: per-file FAILURE banner + non-zero
 * exit but continues over the whole corpus so a single bad file does not
 * mask broader regressions.
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include <sqlite3.h>

#include "diku.h"
#include "diku_sqlite.h"

/* ------------------------------------------------------------------ */
/* File enumeration (copied structurally from test_parser_comprehensive) */
/* ------------------------------------------------------------------ */

typedef struct { char **paths; int count; int cap; } path_list_t;

static void plist_add(path_list_t *l, const char *p) {
    if (l->count >= l->cap) {
        int nc = l->cap ? l->cap * 2 : 32;
        l->paths = (char **)realloc(l->paths, nc * sizeof(char *));
        l->cap = nc;
    }
    l->paths[l->count++] = strdup(p);
}

static void plist_free(path_list_t *l) {
    for (int i = 0; i < l->count; i++) free(l->paths[i]);
    free(l->paths);
    memset(l, 0, sizeof(*l));
}

static void collect(const char *dir, const char *suffix, path_list_t *out) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *de;
    size_t sl = strlen(suffix);
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char buf[4096];
        snprintf(buf, sizeof(buf), "%s/%s", dir, de->d_name);
        struct stat st;
        if (stat(buf, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            collect(buf, suffix, out);
        } else {
            size_t nl = strlen(de->d_name);
            if (nl > sl && strcasecmp(de->d_name + nl - sl, suffix) == 0) {
                plist_add(out, buf);
            }
        }
    }
    closedir(d);
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

/* ------------------------------------------------------------------ */
/* Assertions                                                         */
/* ------------------------------------------------------------------ */

static int sql_count(sqlite3 *db, const char *sql, int *out) {
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return 0;
    int rc = sqlite3_step(st);
    if (rc != SQLITE_ROW) { sqlite3_finalize(st); return 0; }
    *out = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return 1;
}

static int expected_exits(area_t *areas) {
    int n = 0;
    for (area_t *a = areas; a; a = a->next) {
        for (int i = 0; i < a->room_count; i++) {
            for (int d = 0; d < DIKU_MAX_EXITS; d++) {
                if (a->rooms[i].exits[d]) n++;
            }
        }
    }
    return n;
}

static int expected_extra_descs(area_t *areas) {
    int n = 0;
    for (area_t *a = areas; a; a = a->next) {
        for (int i = 0; i < a->room_count; i++) n += a->rooms[i].extra_desc_count;
        for (int i = 0; i < a->item_count; i++) n += a->items[i].extra_desc_count;
    }
    return n;
}

static int expected_affects(area_t *areas) {
    int n = 0;
    for (area_t *a = areas; a; a = a->next) {
        for (int i = 0; i < a->item_count; i++) n += a->items[i].affect_count;
    }
    return n;
}

static int expected_room_mobiles(area_t *areas) {
    int n = 0;
    for (area_t *a = areas; a; a = a->next) {
        for (int i = 0; i < a->room_count; i++) n += a->rooms[i].room_mobile_count;
    }
    return n;
}

static int expected_room_items(area_t *areas) {
    int n = 0;
    for (area_t *a = areas; a; a = a->next) {
        for (int i = 0; i < a->room_count; i++) n += a->rooms[i].room_item_count;
    }
    return n;
}

typedef struct {
    int room_count, mobile_count, item_count;
    int exit_count, extra_desc_count, affect_count;
    int room_mobile_count, room_item_count;
    int resets_count;
    int area_count;
} totals_t;

static totals_t compute_totals(area_t *areas) {
    totals_t t = {0};
    for (area_t *a = areas; a; a = a->next) {
        t.area_count++;
        t.room_count += a->room_count;
        t.mobile_count += a->mobile_count;
        t.item_count += a->item_count;
        t.resets_count += a->resets_line_count;
    }
    t.exit_count = expected_exits(areas);
    t.extra_desc_count = expected_extra_descs(areas);
    t.affect_count = expected_affects(areas);
    t.room_mobile_count = expected_room_mobiles(areas);
    t.room_item_count = expected_room_items(areas);
    return t;
}

/* ------------------------------------------------------------------ */
/* Per-file test                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    int files_ok, files_fail;
    int rooms_seen, mobiles_seen, items_seen;
} summary_t;

static int check_counts(sqlite3 *db, const char *label, totals_t exp) {
    struct { const char *sql; int exp; const char *what; } checks[] = {
        { "SELECT COUNT(*) FROM t_areas",         exp.area_count,       "areas" },
        { "SELECT COUNT(*) FROM t_rooms",         exp.room_count,       "rooms" },
        { "SELECT COUNT(*) FROM t_mobiles",       exp.mobile_count,     "mobiles" },
        { "SELECT COUNT(*) FROM t_items",         exp.item_count,       "items" },
        { "SELECT COUNT(*) FROM t_exits",         exp.exit_count,       "exits" },
        { "SELECT COUNT(*) FROM t_extra_descs",   exp.extra_desc_count, "extra_descs" },
        { "SELECT COUNT(*) FROM t_affects",       exp.affect_count,     "affects" },
        { "SELECT COUNT(*) FROM t_room_mobiles",  exp.room_mobile_count,"room_mobiles" },
        { "SELECT COUNT(*) FROM t_room_items",    exp.room_item_count,  "room_items" },
        { "SELECT COUNT(*) FROM t_resets",        exp.resets_count,     "resets" },
    };
    int failed = 0;
    for (size_t i = 0; i < sizeof(checks)/sizeof(checks[0]); i++) {
        int got = -1;
        if (!sql_count(db, checks[i].sql, &got)) {
            fprintf(stderr, "  [%s] SQL failed: %s\n", label, checks[i].sql);
            failed++;
            continue;
        }
        if (got != checks[i].exp) {
            fprintf(stderr, "  [%s] mismatch: %s SQL=%d expected=%d\n",
                    label, checks[i].what, got, checks[i].exp);
            failed++;
        }
    }
    return failed;
}

static int check_lookup_sample(sqlite3 *db, area_t *areas) {
    /* For each area with at least one room, pick the first room's vnum
     * and verify SELECT name FROM t_rooms WHERE area_id=? AND vnum=?
     * matches the underlying diku_find_room string. */
    int area_id = 0;
    int failed = 0;
    for (area_t *a = areas; a; a = a->next, area_id++) {
        if (a->room_count == 0) continue;
        room_t *r = &a->rooms[0];
        if (r->vnum == INT_MIN) continue;
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db,
            "SELECT name FROM t_rooms WHERE area_id=? AND vnum=?", -1, &st, NULL) != SQLITE_OK) {
            failed++; continue;
        }
        sqlite3_bind_int(st, 1, area_id);
        sqlite3_bind_int(st, 2, r->vnum);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *nm = sqlite3_column_text(st, 0);
            const char *expect = r->name.str ? r->name.str : "";
            if (nm && strcmp((const char *)nm, expect) != 0) {
                fprintf(stderr, "  lookup mismatch: area_id=%d vnum=%d sql='%s' c='%s'\n",
                        area_id, r->vnum, nm, expect);
                failed++;
            }
        } else {
            fprintf(stderr, "  lookup missing: area_id=%d vnum=%d\n", area_id, r->vnum);
            failed++;
        }
        sqlite3_finalize(st);
    }
    return failed;
}

static int test_write_roundtrip(sqlite3 *db, area_t *areas) {
    /* Find a room to poke — first non-empty area, first room */
    area_t *target_area = NULL;
    int target_area_id = 0;
    for (area_t *a = areas; a; a = a->next, target_area_id++) {
        if (a->room_count > 0 && a->rooms[0].vnum != INT_MIN) {
            target_area = a;
            break;
        }
    }
    if (!target_area) return 0; /* nothing to test — vacuous pass */

    room_t *r = &target_area->rooms[0];
    int vnum = r->vnum;
    char original[512] = {0};
    if (r->name.str) {
        strncpy(original, r->name.str, sizeof(original) - 1);
    }

    /* UPDATE via SQL. */
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
        "UPDATE t_rooms SET name='ZZZTESTNAME' WHERE area_id=? AND vnum=?",
        -1, &st, NULL) != SQLITE_OK) {
        fprintf(stderr, "  write test: prepare UPDATE failed: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    sqlite3_bind_int(st, 1, target_area_id);
    sqlite3_bind_int(st, 2, vnum);
    if (sqlite3_step(st) != SQLITE_DONE) {
        fprintf(stderr, "  write test: UPDATE failed: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(st);
        return 1;
    }
    sqlite3_finalize(st);

    /* Verify via C API that the underlying area_t saw the write. */
    room_t *rc = diku_find_room(target_area, vnum);
    if (!rc || !rc->name.str || strcmp(rc->name.str, "ZZZTESTNAME") != 0) {
        fprintf(stderr, "  write test: C-API did not see the write (got '%s')\n",
                rc && rc->name.str ? rc->name.str : "<null>");
        return 1;
    }

    /* Restore via SQL so subsequent counts are unaffected. */
    if (sqlite3_prepare_v2(db,
        "UPDATE t_rooms SET name=? WHERE area_id=? AND vnum=?",
        -1, &st, NULL) != SQLITE_OK) return 1;
    sqlite3_bind_text(st, 1, original, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, target_area_id);
    sqlite3_bind_int(st, 3, vnum);
    sqlite3_step(st);
    sqlite3_finalize(st);
    return 0;
}

static int test_insert_delete(sqlite3 *db, area_t *areas) {
    /* Take an initial count, insert a synthetic room, verify count grew,
     * then delete it and verify count restored. */
    int before = 0;
    if (!sql_count(db, "SELECT COUNT(*) FROM t_rooms", &before)) return 1;
    (void)areas;
    /* INSERT column order matches the table's declared column order
     * (13 columns). Provide values for all. */
    const char *insert_sql =
        "INSERT INTO t_rooms(area_id, vnum, name, description, flags, sector, sector_name,"
        " x, y, z, coord_assigned, extra_desc_count, exit_count) "
        "VALUES (0, 999999, 'inserted', 'insert desc', 0, 0, NULL,"
        " 0, 0, 0, 0, 0, 0)";
    char *err = NULL;
    if (sqlite3_exec(db, insert_sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "  insert test: %s\n", err ? err : "unknown");
        sqlite3_free(err);
        return 1;
    }

    int after = 0;
    if (!sql_count(db, "SELECT COUNT(*) FROM t_rooms", &after)) return 1;
    if (after != before + 1) {
        fprintf(stderr, "  insert test: count %d -> %d (expected +1)\n", before, after);
        return 1;
    }

    /* Verify the row is visible by vnum. */
    int found = 0;
    if (!sql_count(db, "SELECT COUNT(*) FROM t_rooms WHERE vnum=999999", &found)) return 1;
    if (found != 1) {
        fprintf(stderr, "  insert test: could not find inserted row by vnum\n");
        return 1;
    }

    /* DELETE. */
    if (sqlite3_exec(db, "DELETE FROM t_rooms WHERE vnum=999999", NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "  delete test: %s\n", err ? err : "unknown");
        sqlite3_free(err);
        return 1;
    }
    int final_count = 0;
    if (!sql_count(db, "SELECT COUNT(*) FROM t_rooms", &final_count)) return 1;
    if (final_count != before) {
        fprintf(stderr, "  delete test: count %d -> %d (expected %d)\n",
                after, final_count, before);
        return 1;
    }
    return 0;
}

static int test_unload(sqlite3 *db) {
    int ret = 0;
    if (sqlite3_exec(db, "SELECT diku_unload('t')", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "  unload: SQL failed\n");
        return 1;
    }
    /* Subsequent SELECT should fail: table gone. */
    char *err = NULL;
    int rc = sqlite3_exec(db, "SELECT COUNT(*) FROM t_rooms", NULL, NULL, &err);
    if (rc == SQLITE_OK) {
        fprintf(stderr, "  unload: t_rooms still queryable after unload\n");
        ret = 1;
    }
    sqlite3_free(err);
    return ret;
}

static int process_file(const char *path, summary_t *sum, bool do_write_tests) {
    fprintf(stderr, "  %s\n", path);
    sqlite3 *db = NULL;
    if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
        fprintf(stderr, "    FAILURE: could not open in-memory sqlite\n");
        sum->files_fail++;
        return 1;
    }
    if (diku_sqlite_register(db) != SQLITE_OK) {
        fprintf(stderr, "    FAILURE: register failed\n");
        sqlite3_close(db);
        sum->files_fail++;
        return 1;
    }

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, "SELECT diku_load(?, 't')", -1, &st, NULL) != SQLITE_OK) {
        fprintf(stderr, "    FAILURE: prepare diku_load: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        sum->files_fail++;
        return 1;
    }
    sqlite3_bind_text(st, 1, path, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_ROW) {
        fprintf(stderr, "    FAILURE: diku_load: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(st);
        sqlite3_close(db);
        sum->files_fail++;
        return 1;
    }
    sqlite3_finalize(st);

    /* Grab the module's own areas (same pointers backing the vtables)
     * so the write test verifies against the mutated struct, not a copy. */
    area_t *areas = diku_sqlite_get_areas(db, "t");
    if (!areas) {
        fprintf(stderr, "    FAILURE: diku_sqlite_get_areas returned NULL\n");
        sqlite3_close(db);
        sum->files_fail++;
        return 1;
    }

    totals_t exp = compute_totals(areas);
    int failed = check_counts(db, path, exp);
    failed += check_lookup_sample(db, areas);

    if (do_write_tests) {
        failed += test_write_roundtrip(db, areas);
        failed += test_insert_delete(db, areas);
        failed += test_unload(db);
    }

    /* Do NOT free areas — the module owns them and will free them on close
     * or on diku_unload. */
    sqlite3_close(db);

    sum->rooms_seen += exp.room_count;
    sum->mobiles_seen += exp.mobile_count;
    sum->items_seen += exp.item_count;

    if (failed) {
        fprintf(stderr, "    FAILURE: %d check(s) failed for %s\n", failed, path);
        sum->files_fail++;
        return 1;
    }
    sum->files_ok++;
    return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    const char *default_roots[] = { "./data", "./data2" };
    int n_roots = 0;
    const char **roots = NULL;
    if (argc > 1) {
        n_roots = argc - 1;
        roots = (const char **)&argv[1];
    } else {
        n_roots = 2;
        roots = default_roots;
    }

    path_list_t are_list = {0}, pkg_list = {0};
    for (int i = 0; i < n_roots; i++) {
        collect(roots[i], ".are", &are_list);
        collect(roots[i], ".wld", &pkg_list);
    }
    qsort(are_list.paths, are_list.count, sizeof(char *), cmp_str);
    qsort(pkg_list.paths, pkg_list.count, sizeof(char *), cmp_str);

    /* Strip .wld suffix to produce package base paths. */
    for (int i = 0; i < pkg_list.count; i++) {
        size_t l = strlen(pkg_list.paths[i]);
        if (l > 4) pkg_list.paths[i][l - 4] = '\0';
    }

    summary_t sum = {0};

    /* Run the full write-mode test only on the first .are file we find
     * so we don't quadruple runtime; run read-only checks on the rest. */
    bool did_write_tests = false;

    fprintf(stderr, "== .are files (%d) ==\n", are_list.count);
    for (int i = 0; i < are_list.count; i++) {
        bool do_write = !did_write_tests;
        process_file(are_list.paths[i], &sum, do_write);
        if (do_write) did_write_tests = true;
    }

    fprintf(stderr, "== packages (%d) ==\n", pkg_list.count);
    for (int i = 0; i < pkg_list.count; i++) {
        process_file(pkg_list.paths[i], &sum, false);
    }

    fprintf(stderr, "\n== summary ==\n");
    fprintf(stderr, "  files ok:    %d\n", sum.files_ok);
    fprintf(stderr, "  files fail:  %d\n", sum.files_fail);
    fprintf(stderr, "  rooms:       %d\n", sum.rooms_seen);
    fprintf(stderr, "  mobiles:     %d\n", sum.mobiles_seen);
    fprintf(stderr, "  items:       %d\n", sum.items_seen);

    plist_free(&are_list);
    plist_free(&pkg_list);
    return sum.files_fail == 0 ? 0 : 1;
}
