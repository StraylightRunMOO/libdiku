/*
 * diku_sqlite.c — SQLite virtual-table extension for the DikuMUD area parser.
 *
 * See diku_sqlite.h for a summary of the SQL surface. This TU includes only
 * `diku.h` (the umbrella) and does NOT define DIKU_PARSER_IMPLEMENTATION,
 * because the parser implementation is linked in via the diku_impl target.
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#ifdef DIKU_SQLITE_EMBED
#  include <sqlite3.h>
#else
#  include <sqlite3ext.h>
   SQLITE_EXTENSION_INIT1
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "diku.h"
#include "diku_sqlite.h"

/* ------------------------------------------------------------------ */
/* Per-prefix module state                                            */
/* ------------------------------------------------------------------ */

typedef struct diku_module_t diku_module_t;

struct diku_module_t {
    char *prefix;
    area_t *areas;
    diku_context_t *ctx;
    sqlite3 *db;
    diku_module_t *next;
};

/* Registry attached to a sqlite3* connection via sqlite3_set_auxdata /
 * a shared pointer passed as client_data to sqlite3_create_module_v2.
 * We hold a single linked list per connection. */

typedef struct diku_registry_t {
    sqlite3 *db;
    diku_module_t *head;
} diku_registry_t;

static void diku_module_free(diku_module_t *m) {
    if (!m) return;
    if (m->areas) diku_free_all_areas(m->areas);
    if (m->ctx) diku_context_destroy(m->ctx);
    free(m->prefix);
    free(m);
}

static void diku_registry_destroy(void *p) {
    diku_registry_t *reg = (diku_registry_t *)p;
    if (!reg) return;
    diku_module_t *m = reg->head;
    while (m) {
        diku_module_t *next = m->next;
        diku_module_free(m);
        m = next;
    }
    free(reg);
}

/* The registry is attached to each connection as the client_data of the
 * scalar function `diku_load`, with a destructor that tears it down when
 * the connection closes. Inside a function call, retrieve it via
 * sqlite3_user_data(ctx). */

static diku_module_t *diku_registry_find(diku_registry_t *reg, const char *prefix) {
    for (diku_module_t *m = reg->head; m; m = m->next) {
        if (strcmp(m->prefix, prefix) == 0) return m;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Table descriptors — a single generic vtab drives all 10 tables.    */
/* ------------------------------------------------------------------ */

typedef enum {
    DTBL_AREAS = 0,
    DTBL_ROOMS,
    DTBL_MOBILES,
    DTBL_ITEMS,
    DTBL_EXITS,
    DTBL_RESETS,
    DTBL_EXTRA_DESCS,
    DTBL_AFFECTS,
    DTBL_ROOM_MOBILES,
    DTBL_ROOM_ITEMS,
    DTBL_COUNT
} diku_tbl_kind_t;

typedef struct {
    const char *suffix;
    const char *schema;
} diku_tbl_spec_t;

static const diku_tbl_spec_t DIKU_TBL_SPECS[DTBL_COUNT] = {
    [DTBL_AREAS] = { "areas",
        "CREATE TABLE x("
        "area_id INTEGER, name TEXT, filename TEXT, builders TEXT, credits TEXT,"
        "format INTEGER, format_name TEXT,"
        "low_level INTEGER, high_level INTEGER,"
        "low_vnum INTEGER, high_vnum INTEGER,"
        "room_count INTEGER, mobile_count INTEGER, item_count INTEGER)" },
    [DTBL_ROOMS] = { "rooms",
        "CREATE TABLE x("
        "area_id INTEGER, vnum INTEGER, name TEXT, description TEXT,"
        "flags INTEGER, sector INTEGER, sector_name TEXT,"
        "x INTEGER, y INTEGER, z INTEGER, coord_assigned INTEGER,"
        "extra_desc_count INTEGER, exit_count INTEGER)" },
    [DTBL_MOBILES] = { "mobiles",
        "CREATE TABLE x("
        "area_id INTEGER, vnum INTEGER, name TEXT, short_desc TEXT,"
        "long_desc TEXT, description TEXT, level INTEGER, alignment INTEGER,"
        "sex INTEGER, race INTEGER,"
        "act_flags INTEGER, aff_flags INTEGER, off_flags INTEGER,"
        "imm_flags INTEGER, res_flags INTEGER, vuln_flags INTEGER,"
        "hitroll INTEGER, damroll INTEGER,"
        "ac0 INTEGER, ac1 INTEGER, ac2 INTEGER, ac3 INTEGER,"
        "hit_dice INTEGER, hit_size INTEGER, hit_bonus INTEGER,"
        "mana_dice INTEGER, mana_size INTEGER, mana_bonus INTEGER,"
        "damage_dice INTEGER, damage_size INTEGER, damage_bonus INTEGER,"
        "start_pos INTEGER, default_pos INTEGER,"
        "gold INTEGER, silver INTEGER,"
        "form INTEGER, parts INTEGER, size INTEGER, material TEXT)" },
    [DTBL_ITEMS] = { "items",
        "CREATE TABLE x("
        "area_id INTEGER, vnum INTEGER, name TEXT, short_desc TEXT,"
        "long_desc TEXT, description TEXT,"
        "type INTEGER, type_name TEXT,"
        "value0 INTEGER, value1 INTEGER, value2 INTEGER, value3 INTEGER, value4 INTEGER,"
        "weight INTEGER, cost INTEGER, level INTEGER,"
        "extra_flags INTEGER, wear_flags INTEGER, material TEXT)" },
    [DTBL_EXITS] = { "exits",
        "CREATE TABLE x("
        "area_id INTEGER, from_vnum INTEGER, direction INTEGER, direction_name TEXT,"
        "to_vnum INTEGER, to_room_area_id INTEGER,"
        "flags INTEGER, key_vnum INTEGER, description TEXT, keywords TEXT)" },
    [DTBL_RESETS] = { "resets",
        "CREATE TABLE x("
        "area_id INTEGER, line_no INTEGER, cmd TEXT,"
        "arg1 INTEGER, arg2 INTEGER, arg3 INTEGER, arg4 INTEGER, arg5 INTEGER,"
        "raw TEXT)" },
    [DTBL_EXTRA_DESCS] = { "extra_descs",
        "CREATE TABLE x("
        "area_id INTEGER, owner_kind TEXT, owner_vnum INTEGER, idx INTEGER,"
        "keywords TEXT, description TEXT)" },
    [DTBL_AFFECTS] = { "affects",
        "CREATE TABLE x("
        "area_id INTEGER, item_vnum INTEGER, idx INTEGER,"
        "location INTEGER, modifier INTEGER)" },
    [DTBL_ROOM_MOBILES] = { "room_mobiles",
        "CREATE TABLE x("
        "area_id INTEGER, room_vnum INTEGER, idx INTEGER, mobile_vnum INTEGER)" },
    [DTBL_ROOM_ITEMS] = { "room_items",
        "CREATE TABLE x("
        "area_id INTEGER, room_vnum INTEGER, idx INTEGER, item_vnum INTEGER)" },
};

/* ------------------------------------------------------------------ */
/* vtab + cursor structs                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    sqlite3_vtab base;
    diku_module_t *mod;
    diku_tbl_kind_t kind;
} diku_vtab_t;

typedef struct {
    sqlite3_vtab_cursor base;
    diku_vtab_t *vtab;
    sqlite3_int64 rowid;

    /* iteration state */
    area_t *area;
    int area_id;
    int i;   /* primary index into area's array */
    int j;   /* secondary index (exits, extra_descs, affects, room_mobiles, room_items) */
    int k;   /* tertiary — item extra_descs need switching owner_kind */
    bool eof;
} diku_cursor_t;

/* ------------------------------------------------------------------ */
/* Iteration helpers                                                  */
/* ------------------------------------------------------------------ */

static int area_by_id(area_t *head, int id, area_t **out) {
    int n = 0;
    for (area_t *a = head; a; a = a->next, n++) {
        if (n == id) { *out = a; return 1; }
    }
    return 0;
}

static int count_areas(area_t *head) {
    int n = 0;
    for (area_t *a = head; a; a = a->next) n++;
    return n;
}

/* Advance the cursor to the next valid row. Called after i/j are stepped. */
static void cursor_advance(diku_cursor_t *c);

static void cursor_reset(diku_cursor_t *c) {
    c->area = c->vtab->mod->areas;
    c->area_id = 0;
    c->i = 0;
    c->j = 0;
    c->k = 0;
    c->eof = false;
    cursor_advance(c);
}

static void next_area(diku_cursor_t *c) {
    c->area = c->area->next;
    c->area_id++;
    c->i = 0;
    c->j = 0;
    c->k = 0;
}

static void cursor_advance(diku_cursor_t *c) {
    diku_vtab_t *v = c->vtab;
    while (c->area) {
        switch (v->kind) {
        case DTBL_AREAS:
            /* one row per area */
            if (c->i == 0) return;
            next_area(c);
            break;

        case DTBL_ROOMS:
            while (c->i < c->area->room_count) {
                if (c->area->rooms[c->i].vnum != INT_MIN) return;
                c->i++;
            }
            next_area(c);
            break;

        case DTBL_MOBILES:
            while (c->i < c->area->mobile_count) {
                if (c->area->mobiles[c->i].vnum != INT_MIN) return;
                c->i++;
            }
            next_area(c);
            break;

        case DTBL_ITEMS:
            while (c->i < c->area->item_count) {
                if (c->area->items[c->i].vnum != INT_MIN) return;
                c->i++;
            }
            next_area(c);
            break;

        case DTBL_EXITS:
            while (c->i < c->area->room_count) {
                if (c->area->rooms[c->i].vnum == INT_MIN) { c->i++; c->j = 0; continue; }
                while (c->j < DIKU_MAX_EXITS) {
                    if (c->area->rooms[c->i].exits[c->j]) return;
                    c->j++;
                }
                c->i++;
                c->j = 0;
            }
            next_area(c);
            break;

        case DTBL_RESETS:
            if (c->i < c->area->resets_line_count) return;
            next_area(c);
            break;

        case DTBL_EXTRA_DESCS:
            /* k == 0: room extras; k == 1: item extras */
            while (c->k <= 1) {
                if (c->k == 0) {
                    while (c->i < c->area->room_count) {
                        if (c->area->rooms[c->i].vnum == INT_MIN) { c->i++; c->j = 0; continue; }
                        if (c->j < c->area->rooms[c->i].extra_desc_count) return;
                        c->i++;
                        c->j = 0;
                    }
                    c->k = 1;
                    c->i = 0;
                    c->j = 0;
                }
                if (c->k == 1) {
                    while (c->i < c->area->item_count) {
                        if (c->area->items[c->i].vnum == INT_MIN) { c->i++; c->j = 0; continue; }
                        if (c->j < c->area->items[c->i].extra_desc_count) return;
                        c->i++;
                        c->j = 0;
                    }
                    break;
                }
            }
            next_area(c);
            break;

        case DTBL_AFFECTS:
            while (c->i < c->area->item_count) {
                if (c->area->items[c->i].vnum == INT_MIN) { c->i++; c->j = 0; continue; }
                if (c->j < c->area->items[c->i].affect_count) return;
                c->i++;
                c->j = 0;
            }
            next_area(c);
            break;

        case DTBL_ROOM_MOBILES:
            while (c->i < c->area->room_count) {
                if (c->area->rooms[c->i].vnum == INT_MIN) { c->i++; c->j = 0; continue; }
                if (c->j < c->area->rooms[c->i].room_mobile_count) return;
                c->i++;
                c->j = 0;
            }
            next_area(c);
            break;

        case DTBL_ROOM_ITEMS:
            while (c->i < c->area->room_count) {
                if (c->area->rooms[c->i].vnum == INT_MIN) { c->i++; c->j = 0; continue; }
                if (c->j < c->area->rooms[c->i].room_item_count) return;
                c->i++;
                c->j = 0;
            }
            next_area(c);
            break;

        default:
            c->eof = true;
            return;
        }
    }
    c->eof = true;
}

static void cursor_step(diku_cursor_t *c) {
    diku_vtab_t *v = c->vtab;
    switch (v->kind) {
    case DTBL_AREAS:        next_area(c); break;
    case DTBL_ROOMS:
    case DTBL_MOBILES:
    case DTBL_ITEMS:
    case DTBL_RESETS:       c->i++; break;
    case DTBL_EXITS:
    case DTBL_AFFECTS:
    case DTBL_ROOM_MOBILES:
    case DTBL_ROOM_ITEMS:
    case DTBL_EXTRA_DESCS:  c->j++; break;
    default:                c->eof = true; return;
    }
    cursor_advance(c);
}

/* ------------------------------------------------------------------ */
/* SQLite vtable methods                                              */
/* ------------------------------------------------------------------ */

static int diku_vt_connect(sqlite3 *db, void *pAux, int argc, const char *const *argv,
                           sqlite3_vtab **ppVtab, char **pzErr) {
    (void)argc; (void)argv; (void)pzErr;
    diku_vtab_t *vtab = (diku_vtab_t *)pAux;
    /* pAux was set to a heap-allocated placeholder; each connect returns
     * a fresh vtab wrapping it. We recycle the same placeholder because
     * the module registration path passes a per-table descriptor. */
    diku_vtab_t *self = (diku_vtab_t *)sqlite3_malloc(sizeof(*self));
    if (!self) return SQLITE_NOMEM;
    memset(self, 0, sizeof(*self));
    self->mod = vtab->mod;
    self->kind = vtab->kind;
    int rc = sqlite3_declare_vtab(db, DIKU_TBL_SPECS[self->kind].schema);
    if (rc != SQLITE_OK) { sqlite3_free(self); return rc; }
    *ppVtab = &self->base;
    return SQLITE_OK;
}

static int diku_vt_disconnect(sqlite3_vtab *pVtab) {
    sqlite3_free(pVtab);
    return SQLITE_OK;
}

static int diku_vt_best_index(sqlite3_vtab *pVtab, sqlite3_index_info *info) {
    (void)pVtab;
    /* Simple: full scan for everything. SQLite will still push down
     * predicates, and cardinalities in these areas are small
     * (thousands of rows per area, tens of areas). */
    info->estimatedCost = 1000000.0;
    info->estimatedRows = 10000;
    return SQLITE_OK;
}

static int diku_vt_open(sqlite3_vtab *pVtab, sqlite3_vtab_cursor **ppCursor) {
    diku_cursor_t *c = (diku_cursor_t *)sqlite3_malloc(sizeof(*c));
    if (!c) return SQLITE_NOMEM;
    memset(c, 0, sizeof(*c));
    c->vtab = (diku_vtab_t *)pVtab;
    *ppCursor = &c->base;
    return SQLITE_OK;
}

static int diku_vt_close(sqlite3_vtab_cursor *pCursor) {
    sqlite3_free(pCursor);
    return SQLITE_OK;
}

static int diku_vt_filter(sqlite3_vtab_cursor *pCursor, int idxNum, const char *idxStr,
                          int argc, sqlite3_value **argv) {
    (void)idxNum; (void)idxStr; (void)argc; (void)argv;
    diku_cursor_t *c = (diku_cursor_t *)pCursor;
    c->rowid = 0;
    cursor_reset(c);
    return SQLITE_OK;
}

static int diku_vt_next(sqlite3_vtab_cursor *pCursor) {
    diku_cursor_t *c = (diku_cursor_t *)pCursor;
    c->rowid++;
    cursor_step(c);
    return SQLITE_OK;
}

static int diku_vt_eof(sqlite3_vtab_cursor *pCursor) {
    diku_cursor_t *c = (diku_cursor_t *)pCursor;
    return c->eof ? 1 : 0;
}

static int diku_vt_rowid(sqlite3_vtab_cursor *pCursor, sqlite3_int64 *pRowid) {
    diku_cursor_t *c = (diku_cursor_t *)pCursor;
    *pRowid = c->rowid;
    return SQLITE_OK;
}

/* ------------------------------------------------------------------ */
/* Column readers                                                     */
/* ------------------------------------------------------------------ */

static void result_diku_string(sqlite3_context *ctx, const diku_string_t *s) {
    if (!s || !s->str) { sqlite3_result_null(ctx); return; }
    sqlite3_result_text(ctx, s->str, (int)s->len, SQLITE_TRANSIENT);
}

static void result_cstr(sqlite3_context *ctx, const char *s) {
    if (!s) sqlite3_result_null(ctx);
    else sqlite3_result_text(ctx, s, -1, SQLITE_TRANSIENT);
}

static int diku_col_areas(sqlite3_context *ctx, diku_cursor_t *c, int col) {
    area_t *a = c->area;
    switch (col) {
    case 0:  sqlite3_result_int(ctx, c->area_id); break;
    case 1:  result_diku_string(ctx, &a->name); break;
    case 2:  result_diku_string(ctx, &a->filename); break;
    case 3:  result_diku_string(ctx, &a->builders); break;
    case 4:  result_diku_string(ctx, &a->credits); break;
    case 5:  sqlite3_result_int(ctx, (int)a->format); break;
    case 6:  result_cstr(ctx, diku_format_name(a->format)); break;
    case 7:  sqlite3_result_int(ctx, a->low_level); break;
    case 8:  sqlite3_result_int(ctx, a->high_level); break;
    case 9:  sqlite3_result_int(ctx, a->low_vnum); break;
    case 10: sqlite3_result_int(ctx, a->high_vnum); break;
    case 11: sqlite3_result_int(ctx, a->room_count); break;
    case 12: sqlite3_result_int(ctx, a->mobile_count); break;
    case 13: sqlite3_result_int(ctx, a->item_count); break;
    default: sqlite3_result_null(ctx); break;
    }
    return SQLITE_OK;
}

static int count_exits_all(const room_t *r) {
    int n = 0;
    for (int d = 0; d < DIKU_MAX_EXITS; d++) if (r->exits[d]) n++;
    return n;
}

static int diku_col_rooms(sqlite3_context *ctx, diku_cursor_t *c, int col) {
    room_t *r = &c->area->rooms[c->i];
    switch (col) {
    case 0:  sqlite3_result_int(ctx, c->area_id); break;
    case 1:  sqlite3_result_int(ctx, r->vnum); break;
    case 2:  result_diku_string(ctx, &r->name); break;
    case 3:  result_diku_string(ctx, &r->desc); break;
    case 4:  sqlite3_result_int(ctx, r->flags); break;
    case 5:  sqlite3_result_int(ctx, r->sector); break;
    case 6:  result_cstr(ctx, diku_sector_name(r->sector)); break;
    case 7:  sqlite3_result_int(ctx, r->coord.x); break;
    case 8:  sqlite3_result_int(ctx, r->coord.y); break;
    case 9:  sqlite3_result_int(ctx, r->coord.z); break;
    case 10: sqlite3_result_int(ctx, r->coord_assigned ? 1 : 0); break;
    case 11: sqlite3_result_int(ctx, r->extra_desc_count); break;
    case 12: sqlite3_result_int(ctx, count_exits_all(r)); break;
    default: sqlite3_result_null(ctx); break;
    }
    return SQLITE_OK;
}

static int diku_col_mobiles(sqlite3_context *ctx, diku_cursor_t *c, int col) {
    mobile_t *m = &c->area->mobiles[c->i];
    switch (col) {
    case 0:  sqlite3_result_int(ctx, c->area_id); break;
    case 1:  sqlite3_result_int(ctx, m->vnum); break;
    case 2:  result_diku_string(ctx, &m->name); break;
    case 3:  result_diku_string(ctx, &m->short_desc); break;
    case 4:  result_diku_string(ctx, &m->long_desc); break;
    case 5:  result_diku_string(ctx, &m->description); break;
    case 6:  sqlite3_result_int(ctx, m->level); break;
    case 7:  sqlite3_result_int(ctx, m->alignment); break;
    case 8:  sqlite3_result_int(ctx, m->sex); break;
    case 9:  sqlite3_result_int(ctx, m->race); break;
    case 10: sqlite3_result_int64(ctx, (sqlite3_int64)m->act_flags); break;
    case 11: sqlite3_result_int64(ctx, (sqlite3_int64)m->aff_flags); break;
    case 12: sqlite3_result_int64(ctx, (sqlite3_int64)m->off_flags); break;
    case 13: sqlite3_result_int64(ctx, (sqlite3_int64)m->imm_flags); break;
    case 14: sqlite3_result_int64(ctx, (sqlite3_int64)m->res_flags); break;
    case 15: sqlite3_result_int64(ctx, (sqlite3_int64)m->vuln_flags); break;
    case 16: sqlite3_result_int(ctx, m->hitroll); break;
    case 17: sqlite3_result_int(ctx, m->damroll); break;
    case 18: sqlite3_result_int(ctx, m->ac[0]); break;
    case 19: sqlite3_result_int(ctx, m->ac[1]); break;
    case 20: sqlite3_result_int(ctx, m->ac[2]); break;
    case 21: sqlite3_result_int(ctx, m->ac[3]); break;
    case 22: sqlite3_result_int(ctx, m->hit[0]); break;
    case 23: sqlite3_result_int(ctx, m->hit[1]); break;
    case 24: sqlite3_result_int(ctx, m->hit[2]); break;
    case 25: sqlite3_result_int(ctx, m->mana[0]); break;
    case 26: sqlite3_result_int(ctx, m->mana[1]); break;
    case 27: sqlite3_result_int(ctx, m->mana[2]); break;
    case 28: sqlite3_result_int(ctx, m->damage[0]); break;
    case 29: sqlite3_result_int(ctx, m->damage[1]); break;
    case 30: sqlite3_result_int(ctx, m->damage[2]); break;
    case 31: sqlite3_result_int(ctx, m->start_pos); break;
    case 32: sqlite3_result_int(ctx, m->default_pos); break;
    case 33: sqlite3_result_int64(ctx, (sqlite3_int64)m->gold); break;
    case 34: sqlite3_result_int64(ctx, (sqlite3_int64)m->silver); break;
    case 35: sqlite3_result_int(ctx, m->form); break;
    case 36: sqlite3_result_int(ctx, m->parts); break;
    case 37: sqlite3_result_int(ctx, m->size); break;
    case 38: result_diku_string(ctx, &m->material); break;
    default: sqlite3_result_null(ctx); break;
    }
    return SQLITE_OK;
}

static int diku_col_items(sqlite3_context *ctx, diku_cursor_t *c, int col) {
    item_t *it = &c->area->items[c->i];
    switch (col) {
    case 0:  sqlite3_result_int(ctx, c->area_id); break;
    case 1:  sqlite3_result_int(ctx, it->vnum); break;
    case 2:  result_diku_string(ctx, &it->name); break;
    case 3:  result_diku_string(ctx, &it->short_desc); break;
    case 4:  result_diku_string(ctx, &it->long_desc); break;
    case 5:  result_diku_string(ctx, &it->description); break;
    case 6:  sqlite3_result_int(ctx, it->type); break;
    case 7:  result_cstr(ctx, diku_item_type_name(it->type)); break;
    case 8:  sqlite3_result_int(ctx, it->value[0]); break;
    case 9:  sqlite3_result_int(ctx, it->value[1]); break;
    case 10: sqlite3_result_int(ctx, it->value[2]); break;
    case 11: sqlite3_result_int(ctx, it->value[3]); break;
    case 12: sqlite3_result_int(ctx, it->value[4]); break;
    case 13: sqlite3_result_int(ctx, it->weight); break;
    case 14: sqlite3_result_int(ctx, it->cost); break;
    case 15: sqlite3_result_int(ctx, it->level); break;
    case 16: sqlite3_result_int64(ctx, (sqlite3_int64)it->extra_flags); break;
    case 17: sqlite3_result_int64(ctx, (sqlite3_int64)it->wear_flags); break;
    case 18: result_diku_string(ctx, &it->material); break;
    default: sqlite3_result_null(ctx); break;
    }
    return SQLITE_OK;
}

/* Resolve which area a to_room pointer belongs to. Linear over the
 * area list but only called on demand. */
static int resolve_to_room_area_id(diku_module_t *mod, const room_t *target) {
    if (!target) return -1;
    int idx = 0;
    for (area_t *a = mod->areas; a; a = a->next, idx++) {
        if (a->room_count > 0 &&
            target >= &a->rooms[0] &&
            target <= &a->rooms[a->room_count - 1]) {
            return idx;
        }
    }
    return -1;
}

static int diku_col_exits(sqlite3_context *ctx, diku_cursor_t *c, int col) {
    room_t *r = &c->area->rooms[c->i];
    exit_t *e = r->exits[c->j];
    switch (col) {
    case 0:  sqlite3_result_int(ctx, c->area_id); break;
    case 1:  sqlite3_result_int(ctx, r->vnum); break;
    case 2:  sqlite3_result_int(ctx, c->j); break;
    case 3:  result_cstr(ctx, diku_dir_name(c->j)); break;
    case 4:  sqlite3_result_int(ctx, e->to_vnum); break;
    case 5:  sqlite3_result_int(ctx, resolve_to_room_area_id(c->vtab->mod, e->to_room)); break;
    case 6:  sqlite3_result_int(ctx, e->flags); break;
    case 7:  sqlite3_result_int(ctx, e->key_vnum); break;
    case 8:  result_diku_string(ctx, &e->desc); break;
    case 9:  result_diku_string(ctx, &e->keywords); break;
    default: sqlite3_result_null(ctx); break;
    }
    return SQLITE_OK;
}

static int diku_col_resets(sqlite3_context *ctx, diku_cursor_t *c, int col) {
    const char *line = c->area->resets_raw[c->i];
    if (!line) line = "";
    if (col == 0) { sqlite3_result_int(ctx, c->area_id); return SQLITE_OK; }
    if (col == 1) { sqlite3_result_int(ctx, c->i); return SQLITE_OK; }
    if (col == 8) { result_cstr(ctx, line); return SQLITE_OK; }
    char cmd = 0;
    int arg[5] = {0,0,0,0,0};
    int n = sscanf(line, " %c %d %d %d %d %d",
                   &cmd, &arg[0], &arg[1], &arg[2], &arg[3], &arg[4]);
    if (col == 2) {
        if (n < 1) sqlite3_result_null(ctx);
        else { char buf[2] = { cmd, 0 }; sqlite3_result_text(ctx, buf, 1, SQLITE_TRANSIENT); }
    } else if (col >= 3 && col <= 7) {
        int idx = col - 3;
        if (n < idx + 2) sqlite3_result_null(ctx);
        else sqlite3_result_int(ctx, arg[idx]);
    } else {
        sqlite3_result_null(ctx);
    }
    return SQLITE_OK;
}

static int diku_col_extra_descs(sqlite3_context *ctx, diku_cursor_t *c, int col) {
    diku_string_t *kw = NULL, *desc = NULL;
    const char *owner_kind = "room";
    int owner_vnum = 0;
    if (c->k == 0) {
        room_t *r = &c->area->rooms[c->i];
        owner_kind = "room";
        owner_vnum = r->vnum;
        kw = &r->extra_descs[c->j].keywords;
        desc = &r->extra_descs[c->j].desc;
    } else {
        item_t *it = &c->area->items[c->i];
        owner_kind = "item";
        owner_vnum = it->vnum;
        kw = &it->extra_descs[c->j].keywords;
        desc = &it->extra_descs[c->j].desc;
    }
    switch (col) {
    case 0: sqlite3_result_int(ctx, c->area_id); break;
    case 1: result_cstr(ctx, owner_kind); break;
    case 2: sqlite3_result_int(ctx, owner_vnum); break;
    case 3: sqlite3_result_int(ctx, c->j); break;
    case 4: result_diku_string(ctx, kw); break;
    case 5: result_diku_string(ctx, desc); break;
    default: sqlite3_result_null(ctx); break;
    }
    return SQLITE_OK;
}

static int diku_col_affects(sqlite3_context *ctx, diku_cursor_t *c, int col) {
    item_t *it = &c->area->items[c->i];
    switch (col) {
    case 0: sqlite3_result_int(ctx, c->area_id); break;
    case 1: sqlite3_result_int(ctx, it->vnum); break;
    case 2: sqlite3_result_int(ctx, c->j); break;
    case 3: sqlite3_result_int(ctx, it->affects[c->j].location); break;
    case 4: sqlite3_result_int(ctx, it->affects[c->j].modifier); break;
    default: sqlite3_result_null(ctx); break;
    }
    return SQLITE_OK;
}

static int diku_col_room_mobiles(sqlite3_context *ctx, diku_cursor_t *c, int col) {
    room_t *r = &c->area->rooms[c->i];
    mobile_t *m = r->room_mobiles ? r->room_mobiles[c->j] : NULL;
    switch (col) {
    case 0: sqlite3_result_int(ctx, c->area_id); break;
    case 1: sqlite3_result_int(ctx, r->vnum); break;
    case 2: sqlite3_result_int(ctx, c->j); break;
    case 3: sqlite3_result_int(ctx, m ? m->vnum : -1); break;
    default: sqlite3_result_null(ctx); break;
    }
    return SQLITE_OK;
}

static int diku_col_room_items(sqlite3_context *ctx, diku_cursor_t *c, int col) {
    room_t *r = &c->area->rooms[c->i];
    item_t *it = r->room_items ? r->room_items[c->j] : NULL;
    switch (col) {
    case 0: sqlite3_result_int(ctx, c->area_id); break;
    case 1: sqlite3_result_int(ctx, r->vnum); break;
    case 2: sqlite3_result_int(ctx, c->j); break;
    case 3: sqlite3_result_int(ctx, it ? it->vnum : -1); break;
    default: sqlite3_result_null(ctx); break;
    }
    return SQLITE_OK;
}

static int diku_vt_column(sqlite3_vtab_cursor *pCursor, sqlite3_context *ctx, int col) {
    diku_cursor_t *c = (diku_cursor_t *)pCursor;
    switch (c->vtab->kind) {
    case DTBL_AREAS:        return diku_col_areas(ctx, c, col);
    case DTBL_ROOMS:        return diku_col_rooms(ctx, c, col);
    case DTBL_MOBILES:      return diku_col_mobiles(ctx, c, col);
    case DTBL_ITEMS:        return diku_col_items(ctx, c, col);
    case DTBL_EXITS:        return diku_col_exits(ctx, c, col);
    case DTBL_RESETS:       return diku_col_resets(ctx, c, col);
    case DTBL_EXTRA_DESCS:  return diku_col_extra_descs(ctx, c, col);
    case DTBL_AFFECTS:      return diku_col_affects(ctx, c, col);
    case DTBL_ROOM_MOBILES: return diku_col_room_mobiles(ctx, c, col);
    case DTBL_ROOM_ITEMS:   return diku_col_room_items(ctx, c, col);
    default: sqlite3_result_null(ctx); return SQLITE_OK;
    }
}

/* ------------------------------------------------------------------ */
/* Writes: xUpdate                                                    */
/* ------------------------------------------------------------------ */

/* Rowid encoding: not all vtables have a natural stable rowid, so we use
 * a per-cursor synthetic one — but xUpdate operates on the caller's rowid
 * value. We instead treat rowid == (area_id * BASE) + primary_index for
 * scalar tables, and reject writes for child tables that can't map back
 * without extra state. */

#define ROWID_BASE 1000000LL

static int locate_by_rowid(diku_vtab_t *v, sqlite3_int64 rowid,
                           area_t **out_area, int *out_i) {
    int area_id = (int)(rowid / ROWID_BASE);
    int i = (int)(rowid % ROWID_BASE);
    area_t *a = NULL;
    if (!area_by_id(v->mod->areas, area_id, &a)) return 0;
    *out_area = a;
    *out_i = i;
    return 1;
}

static int set_diku_str(area_t *area, diku_string_t *dst, sqlite3_value *val) {
    if (sqlite3_value_type(val) == SQLITE_NULL) {
        dst->str = NULL;
        dst->len = 0;
        return SQLITE_OK;
    }
    const char *s = (const char *)sqlite3_value_text(val);
    int n = sqlite3_value_bytes(val);
    if (!s) { dst->str = NULL; dst->len = 0; return SQLITE_OK; }
    *dst = diku_arena_strndup(area->arena, s, (size_t)n);
    return dst->str ? SQLITE_OK : SQLITE_NOMEM;
}

static int update_area_row(diku_vtab_t *v, sqlite3_int64 rowid,
                           int argc, sqlite3_value **argv) {
    (void)argc;
    area_t *a = NULL;
    if (!area_by_id(v->mod->areas, (int)rowid, &a)) return SQLITE_ERROR;
    /* argv[0] = old rowid, argv[1] = new rowid, argv[2+] = columns */
    if (sqlite3_value_type(argv[1]) != SQLITE_NULL &&
        sqlite3_value_int64(argv[1]) != rowid) {
        v->base.zErrMsg = sqlite3_mprintf("cannot change area_id after insert");
        return SQLITE_ERROR;
    }
    /* column order: area_id, name, filename, builders, credits, format,
     * format_name, low_level, high_level, low_vnum, high_vnum,
     * room_count, mobile_count, item_count */
    int rc;
    if ((rc = set_diku_str(a, &a->name, argv[2 + 1])) != SQLITE_OK) return rc;
    if ((rc = set_diku_str(a, &a->filename, argv[2 + 2])) != SQLITE_OK) return rc;
    if ((rc = set_diku_str(a, &a->builders, argv[2 + 3])) != SQLITE_OK) return rc;
    if ((rc = set_diku_str(a, &a->credits, argv[2 + 4])) != SQLITE_OK) return rc;
    a->format = (diku_format_t)sqlite3_value_int(argv[2 + 5]);
    a->low_level = sqlite3_value_int(argv[2 + 7]);
    a->high_level = sqlite3_value_int(argv[2 + 8]);
    a->low_vnum = sqlite3_value_int(argv[2 + 9]);
    a->high_vnum = sqlite3_value_int(argv[2 + 10]);
    return SQLITE_OK;
}

static int update_room_row(diku_vtab_t *v, sqlite3_int64 rowid,
                           int argc, sqlite3_value **argv, bool is_insert) {
    (void)argc;
    area_t *a = NULL; int i = 0;
    if (is_insert) {
        /* INSERT: append to first area if only one, else require area_id */
        int area_id = sqlite3_value_int(argv[2 + 0]);
        if (!area_by_id(v->mod->areas, area_id, &a)) {
            v->base.zErrMsg = sqlite3_mprintf("no area with area_id=%d", area_id);
            return SQLITE_ERROR;
        }
        /* Grow the rooms array by one. */
        int cap = a->room_count + 1;
        room_t *newr = (room_t *)diku_arena_alloc_aligned(
            a->arena, sizeof(room_t) * cap, 64);
        if (!newr) return SQLITE_NOMEM;
        if (a->room_count > 0) memcpy(newr, a->rooms, sizeof(room_t) * a->room_count);
        memset(&newr[a->room_count], 0, sizeof(room_t));
        a->rooms = newr;
        i = a->room_count++;
    } else {
        if (!locate_by_rowid(v, rowid, &a, &i)) return SQLITE_ERROR;
        if (i < 0 || i >= a->room_count) return SQLITE_ERROR;
    }
    room_t *r = &a->rooms[i];
    if (!is_insert && sqlite3_value_type(argv[1]) != SQLITE_NULL &&
        sqlite3_value_int64(argv[1]) != rowid) {
        v->base.zErrMsg = sqlite3_mprintf("cannot change rowid of rooms row");
        return SQLITE_ERROR;
    }
    /* columns: area_id, vnum, name, description, flags, sector, sector_name,
     *          x, y, z, coord_assigned, extra_desc_count, exit_count */
    r->vnum = sqlite3_value_int(argv[2 + 1]);
    int rc;
    if ((rc = set_diku_str(a, &r->name, argv[2 + 2])) != SQLITE_OK) return rc;
    if ((rc = set_diku_str(a, &r->desc, argv[2 + 3])) != SQLITE_OK) return rc;
    r->flags = sqlite3_value_int(argv[2 + 4]);
    r->sector = sqlite3_value_int(argv[2 + 5]);
    r->coord.x = sqlite3_value_int(argv[2 + 7]);
    r->coord.y = sqlite3_value_int(argv[2 + 8]);
    r->coord.z = sqlite3_value_int(argv[2 + 9]);
    r->coord_assigned = sqlite3_value_int(argv[2 + 10]) != 0;
    if (is_insert) diku_build_vnum_hash(a);
    return SQLITE_OK;
}

static int update_mob_row(diku_vtab_t *v, sqlite3_int64 rowid,
                          int argc, sqlite3_value **argv, bool is_insert) {
    (void)argc;
    area_t *a = NULL; int i = 0;
    if (is_insert) {
        int area_id = sqlite3_value_int(argv[2 + 0]);
        if (!area_by_id(v->mod->areas, area_id, &a)) {
            v->base.zErrMsg = sqlite3_mprintf("no area with area_id=%d", area_id);
            return SQLITE_ERROR;
        }
        int cap = a->mobile_count + 1;
        mobile_t *nm = (mobile_t *)diku_arena_alloc_aligned(
            a->arena, sizeof(mobile_t) * cap, 64);
        if (!nm) return SQLITE_NOMEM;
        if (a->mobile_count > 0) memcpy(nm, a->mobiles, sizeof(mobile_t) * a->mobile_count);
        memset(&nm[a->mobile_count], 0, sizeof(mobile_t));
        a->mobiles = nm;
        i = a->mobile_count++;
    } else {
        if (!locate_by_rowid(v, rowid, &a, &i)) return SQLITE_ERROR;
        if (i < 0 || i >= a->mobile_count) return SQLITE_ERROR;
    }
    mobile_t *m = &a->mobiles[i];
    m->vnum = sqlite3_value_int(argv[2 + 1]);
    int rc;
    if ((rc = set_diku_str(a, &m->name, argv[2 + 2])) != SQLITE_OK) return rc;
    if ((rc = set_diku_str(a, &m->short_desc, argv[2 + 3])) != SQLITE_OK) return rc;
    if ((rc = set_diku_str(a, &m->long_desc, argv[2 + 4])) != SQLITE_OK) return rc;
    if ((rc = set_diku_str(a, &m->description, argv[2 + 5])) != SQLITE_OK) return rc;
    m->level = sqlite3_value_int(argv[2 + 6]);
    m->alignment = sqlite3_value_int(argv[2 + 7]);
    m->sex = sqlite3_value_int(argv[2 + 8]);
    m->race = sqlite3_value_int(argv[2 + 9]);
    m->act_flags = (uint32_t)sqlite3_value_int64(argv[2 + 10]);
    m->aff_flags = (uint32_t)sqlite3_value_int64(argv[2 + 11]);
    m->off_flags = (uint32_t)sqlite3_value_int64(argv[2 + 12]);
    m->imm_flags = (uint32_t)sqlite3_value_int64(argv[2 + 13]);
    m->res_flags = (uint32_t)sqlite3_value_int64(argv[2 + 14]);
    m->vuln_flags = (uint32_t)sqlite3_value_int64(argv[2 + 15]);
    m->hitroll = sqlite3_value_int(argv[2 + 16]);
    m->damroll = sqlite3_value_int(argv[2 + 17]);
    for (int k = 0; k < 4; k++) m->ac[k] = sqlite3_value_int(argv[2 + 18 + k]);
    for (int k = 0; k < 3; k++) m->hit[k] = sqlite3_value_int(argv[2 + 22 + k]);
    for (int k = 0; k < 3; k++) m->mana[k] = sqlite3_value_int(argv[2 + 25 + k]);
    for (int k = 0; k < 3; k++) m->damage[k] = sqlite3_value_int(argv[2 + 28 + k]);
    m->start_pos = sqlite3_value_int(argv[2 + 31]);
    m->default_pos = sqlite3_value_int(argv[2 + 32]);
    m->gold = sqlite3_value_int64(argv[2 + 33]);
    m->silver = sqlite3_value_int64(argv[2 + 34]);
    m->form = sqlite3_value_int(argv[2 + 35]);
    m->parts = sqlite3_value_int(argv[2 + 36]);
    m->size = sqlite3_value_int(argv[2 + 37]);
    if ((rc = set_diku_str(a, &m->material, argv[2 + 38])) != SQLITE_OK) return rc;
    return SQLITE_OK;
}

static int update_item_row(diku_vtab_t *v, sqlite3_int64 rowid,
                           int argc, sqlite3_value **argv, bool is_insert) {
    (void)argc;
    area_t *a = NULL; int i = 0;
    if (is_insert) {
        int area_id = sqlite3_value_int(argv[2 + 0]);
        if (!area_by_id(v->mod->areas, area_id, &a)) {
            v->base.zErrMsg = sqlite3_mprintf("no area with area_id=%d", area_id);
            return SQLITE_ERROR;
        }
        int cap = a->item_count + 1;
        item_t *ni = (item_t *)diku_arena_alloc_aligned(
            a->arena, sizeof(item_t) * cap, 64);
        if (!ni) return SQLITE_NOMEM;
        if (a->item_count > 0) memcpy(ni, a->items, sizeof(item_t) * a->item_count);
        memset(&ni[a->item_count], 0, sizeof(item_t));
        a->items = ni;
        i = a->item_count++;
    } else {
        if (!locate_by_rowid(v, rowid, &a, &i)) return SQLITE_ERROR;
        if (i < 0 || i >= a->item_count) return SQLITE_ERROR;
    }
    item_t *it = &a->items[i];
    it->vnum = sqlite3_value_int(argv[2 + 1]);
    int rc;
    if ((rc = set_diku_str(a, &it->name, argv[2 + 2])) != SQLITE_OK) return rc;
    if ((rc = set_diku_str(a, &it->short_desc, argv[2 + 3])) != SQLITE_OK) return rc;
    if ((rc = set_diku_str(a, &it->long_desc, argv[2 + 4])) != SQLITE_OK) return rc;
    if ((rc = set_diku_str(a, &it->description, argv[2 + 5])) != SQLITE_OK) return rc;
    it->type = sqlite3_value_int(argv[2 + 6]);
    for (int k = 0; k < 5; k++) it->value[k] = sqlite3_value_int(argv[2 + 8 + k]);
    it->weight = sqlite3_value_int(argv[2 + 13]);
    it->cost = sqlite3_value_int(argv[2 + 14]);
    it->level = sqlite3_value_int(argv[2 + 15]);
    it->extra_flags = (uint32_t)sqlite3_value_int64(argv[2 + 16]);
    it->wear_flags = (uint32_t)sqlite3_value_int64(argv[2 + 17]);
    if ((rc = set_diku_str(a, &it->material, argv[2 + 18])) != SQLITE_OK) return rc;
    return SQLITE_OK;
}

static int delete_soft(diku_vtab_t *v, sqlite3_int64 rowid) {
    area_t *a = NULL; int i = 0;
    if (!locate_by_rowid(v, rowid, &a, &i)) return SQLITE_ERROR;
    switch (v->kind) {
    case DTBL_ROOMS:
        if (i < 0 || i >= a->room_count) return SQLITE_ERROR;
        a->rooms[i].vnum = INT_MIN;
        break;
    case DTBL_MOBILES:
        if (i < 0 || i >= a->mobile_count) return SQLITE_ERROR;
        a->mobiles[i].vnum = INT_MIN;
        break;
    case DTBL_ITEMS:
        if (i < 0 || i >= a->item_count) return SQLITE_ERROR;
        a->items[i].vnum = INT_MIN;
        break;
    default:
        v->base.zErrMsg = sqlite3_mprintf("DELETE not supported on this table");
        return SQLITE_ERROR;
    }
    return SQLITE_OK;
}

static int diku_vt_update(sqlite3_vtab *pVtab, int argc, sqlite3_value **argv,
                          sqlite3_int64 *pRowid) {
    diku_vtab_t *v = (diku_vtab_t *)pVtab;
    /* argc == 1: DELETE; argv[0] = rowid.
     * argc > 1 and argv[0] == NULL: INSERT.
     * argc > 1 and argv[0] != NULL: UPDATE. */
    if (argc == 1) {
        sqlite3_int64 rowid = sqlite3_value_int64(argv[0]);
        return delete_soft(v, rowid);
    }
    bool is_insert = (sqlite3_value_type(argv[0]) == SQLITE_NULL);
    sqlite3_int64 rowid = is_insert ? 0 : sqlite3_value_int64(argv[0]);
    if (is_insert) {
        switch (v->kind) {
        case DTBL_AREAS:
            v->base.zErrMsg = sqlite3_mprintf("INSERT into areas is not supported");
            return SQLITE_ERROR;
        case DTBL_ROOMS:
        case DTBL_MOBILES:
        case DTBL_ITEMS:
            break;
        default:
            v->base.zErrMsg = sqlite3_mprintf(
                "INSERT not supported on this table");
            return SQLITE_ERROR;
        }
    }
    int rc = SQLITE_OK;
    switch (v->kind) {
    case DTBL_AREAS:
        if (is_insert) return SQLITE_ERROR;
        rc = update_area_row(v, rowid, argc, argv);
        break;
    case DTBL_ROOMS:
        rc = update_room_row(v, rowid, argc, argv, is_insert);
        if (rc == SQLITE_OK && pRowid && is_insert) {
            /* Recompute encoded rowid using new tail slot. */
            /* Not strictly needed; SQLite will ignore for INSERT with rowid handling. */
        }
        break;
    case DTBL_MOBILES:
        rc = update_mob_row(v, rowid, argc, argv, is_insert);
        break;
    case DTBL_ITEMS:
        rc = update_item_row(v, rowid, argc, argv, is_insert);
        break;
    default:
        v->base.zErrMsg = sqlite3_mprintf(
            "UPDATE/INSERT not supported on this table");
        return SQLITE_ERROR;
    }
    return rc;
}

/* ------------------------------------------------------------------ */
/* Module method table                                                */
/* ------------------------------------------------------------------ */

static sqlite3_module diku_module_def = {
    /* iVersion    */ 1,
    /* xCreate     */ diku_vt_connect,
    /* xConnect    */ diku_vt_connect,
    /* xBestIndex  */ diku_vt_best_index,
    /* xDisconnect */ diku_vt_disconnect,
    /* xDestroy    */ diku_vt_disconnect,
    /* xOpen       */ diku_vt_open,
    /* xClose      */ diku_vt_close,
    /* xFilter     */ diku_vt_filter,
    /* xNext       */ diku_vt_next,
    /* xEof        */ diku_vt_eof,
    /* xColumn     */ diku_vt_column,
    /* xRowid      */ diku_vt_rowid,
    /* xUpdate     */ diku_vt_update,
    /* xBegin      */ 0,
    /* xSync       */ 0,
    /* xCommit     */ 0,
    /* xRollback   */ 0,
    /* xFindFunc   */ 0,
    /* xRename     */ 0,
};

/* ------------------------------------------------------------------ */
/* Registering all tables for one module                              */
/* ------------------------------------------------------------------ */

/* Pool of per-table descriptors; xCreate/xConnect gets a pointer to one
 * of these via pAux. Freed when the module is unloaded. */
static void free_pool_entry(void *p) { free(p); }

static int register_module_tables(diku_module_t *m) {
    for (int k = 0; k < DTBL_COUNT; k++) {
        char tbl[128];
        snprintf(tbl, sizeof(tbl), "%s_%s", m->prefix, DIKU_TBL_SPECS[k].suffix);
        diku_vtab_t *desc = (diku_vtab_t *)calloc(1, sizeof(*desc));
        if (!desc) return SQLITE_NOMEM;
        desc->mod = m;
        desc->kind = (diku_tbl_kind_t)k;
        char mod_name[128];
        snprintf(mod_name, sizeof(mod_name), "%s_%s_mod", m->prefix, DIKU_TBL_SPECS[k].suffix);
        int rc = sqlite3_create_module_v2(m->db, mod_name, &diku_module_def, desc, free_pool_entry);
        if (rc != SQLITE_OK) return rc;
        char sql[256];
        snprintf(sql, sizeof(sql), "CREATE VIRTUAL TABLE temp.%s USING %s()", tbl, mod_name);
        char *err = NULL;
        rc = sqlite3_exec(m->db, sql, NULL, NULL, &err);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "diku_sqlite: CREATE VIRTUAL TABLE %s failed: %s\n", tbl, err ? err : "");
            sqlite3_free(err);
            return rc;
        }
    }
    return SQLITE_OK;
}

static int drop_module_tables(diku_module_t *m) {
    for (int k = 0; k < DTBL_COUNT; k++) {
        char sql[256];
        snprintf(sql, sizeof(sql), "DROP TABLE IF EXISTS temp.%s_%s",
                 m->prefix, DIKU_TBL_SPECS[k].suffix);
        sqlite3_exec(m->db, sql, NULL, NULL, NULL);
    }
    return SQLITE_OK;
}

/* ------------------------------------------------------------------ */
/* Scalar functions                                                   */
/* ------------------------------------------------------------------ */

static void fn_diku_load(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    if (argc != 2) { sqlite3_result_error(ctx, "diku_load(path, prefix)", -1); return; }
    const char *path = (const char *)sqlite3_value_text(argv[0]);
    const char *prefix = (const char *)sqlite3_value_text(argv[1]);
    if (!path || !prefix) { sqlite3_result_error(ctx, "null argument", -1); return; }

    sqlite3 *db = sqlite3_context_db_handle(ctx);
    diku_registry_t *reg = (diku_registry_t *)sqlite3_user_data(ctx);
    if (!reg) { sqlite3_result_error_nomem(ctx); return; }
    if (diku_registry_find(reg, prefix)) {
        sqlite3_result_error(ctx, "prefix already loaded; call diku_unload first", -1);
        return;
    }

    diku_module_t *m = (diku_module_t *)calloc(1, sizeof(*m));
    if (!m) { sqlite3_result_error_nomem(ctx); return; }
    m->prefix = strdup(prefix);
    m->ctx = diku_context_create();
    m->db = db;
    if (!m->prefix || !m->ctx) {
        diku_module_free(m);
        sqlite3_result_error_nomem(ctx);
        return;
    }

    m->areas = diku_parse_path(m->ctx, path);
    if (!m->areas) {
        /* Fall back to package base-path semantics (path with no extension
         * but with matching .wld/.mob/.obj/.zon siblings). */
        m->areas = diku_parse_package(m->ctx, path);
    }
    if (!m->areas) {
        diku_module_free(m);
        char *msg = sqlite3_mprintf("failed to parse path: %s", path);
        sqlite3_result_error(ctx, msg, -1);
        sqlite3_free(msg);
        return;
    }
    diku_resolve_graph_global(m->ctx, m->areas);

    /* Prepend to registry BEFORE creating tables so vtabs see it. */
    m->next = reg->head;
    reg->head = m;

    int rc = register_module_tables(m);
    if (rc != SQLITE_OK) {
        reg->head = m->next;
        diku_module_free(m);
        sqlite3_result_error_code(ctx, rc);
        return;
    }

    sqlite3_result_int(ctx, count_areas(m->areas));
}

static void fn_diku_unload(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    if (argc != 1) { sqlite3_result_error(ctx, "diku_unload(prefix)", -1); return; }
    const char *prefix = (const char *)sqlite3_value_text(argv[0]);
    if (!prefix) { sqlite3_result_error(ctx, "null prefix", -1); return; }
    diku_registry_t *reg = (diku_registry_t *)sqlite3_user_data(ctx);
    if (!reg) { sqlite3_result_int(ctx, 0); return; }
    diku_module_t **pp = &reg->head;
    while (*pp) {
        if (strcmp((*pp)->prefix, prefix) == 0) {
            diku_module_t *m = *pp;
            drop_module_tables(m);
            *pp = m->next;
            diku_module_free(m);
            sqlite3_result_int(ctx, 1);
            return;
        }
        pp = &(*pp)->next;
    }
    sqlite3_result_int(ctx, 0);
}

static void fn_diku_reapply_resets(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    if (argc != 1) { sqlite3_result_error(ctx, "diku_reapply_resets(prefix)", -1); return; }
    const char *prefix = (const char *)sqlite3_value_text(argv[0]);
    diku_registry_t *reg = (diku_registry_t *)sqlite3_user_data(ctx);
    if (!reg) { sqlite3_result_int(ctx, 0); return; }
    diku_module_t *m = diku_registry_find(reg, prefix);
    if (!m) { sqlite3_result_int(ctx, 0); return; }
    int n = 0;
    for (area_t *a = m->areas; a; a = a->next) {
        /* Clear room-level derived state before re-parsing. */
        for (int i = 0; i < a->room_count; i++) {
            a->rooms[i].room_mobiles = NULL;
            a->rooms[i].room_mobile_count = 0;
            a->rooms[i].room_items = NULL;
            a->rooms[i].room_item_count = 0;
        }
        for (int i = 0; i < a->mobile_count; i++) {
            a->mobiles[i].inventory = NULL;
            a->mobiles[i].inventory_count = 0;
        }
        diku_parse_resets(a);
        n++;
    }
    sqlite3_result_int(ctx, n);
}

static void fn_diku_dir_name(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    (void)argc;
    result_cstr(ctx, diku_dir_name(sqlite3_value_int(argv[0])));
}
static void fn_diku_sector_name(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    (void)argc;
    result_cstr(ctx, diku_sector_name(sqlite3_value_int(argv[0])));
}
static void fn_diku_item_type_name(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    (void)argc;
    result_cstr(ctx, diku_item_type_name(sqlite3_value_int(argv[0])));
}
static void fn_diku_format_name(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    (void)argc;
    result_cstr(ctx, diku_format_name((diku_format_t)sqlite3_value_int(argv[0])));
}

/* Internal helper used by test/embedding: returns the head area_t*
 * for a prefix, encoded as a 64-bit integer. NULL prefix -> 0. */
static void fn_diku_get_areas_ptr(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    (void)argc;
    const char *prefix = (const char *)sqlite3_value_text(argv[0]);
    diku_registry_t *reg = (diku_registry_t *)sqlite3_user_data(ctx);
    if (!reg || !prefix) { sqlite3_result_int64(ctx, 0); return; }
    diku_module_t *m = diku_registry_find(reg, prefix);
    sqlite3_result_int64(ctx, m ? (sqlite3_int64)(uintptr_t)m->areas : 0);
}

struct area_t *diku_sqlite_get_areas(sqlite3 *db, const char *prefix) {
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, "SELECT diku_get_areas_ptr(?)", -1, &st, NULL) != SQLITE_OK) {
        return NULL;
    }
    sqlite3_bind_text(st, 1, prefix, -1, SQLITE_TRANSIENT);
    struct area_t *result = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        sqlite3_int64 v = sqlite3_column_int64(st, 0);
        result = (struct area_t *)(uintptr_t)v;
    }
    sqlite3_finalize(st);
    return result;
}

/* ------------------------------------------------------------------ */
/* Public registration                                                */
/* ------------------------------------------------------------------ */

int diku_sqlite_register(sqlite3 *db) {
    int rc;
    diku_registry_t *reg = (diku_registry_t *)calloc(1, sizeof(*reg));
    if (!reg) return SQLITE_NOMEM;
    reg->db = db;

    /* Register the three registry-owning functions. Only the FIRST one
     * gets the destructor; SQLite calls it exactly once when the
     * connection is closed, which is when we want to tear down. The other
     * two share the pointer (no ownership). */
    rc = sqlite3_create_function_v2(db, "diku_load", 2, SQLITE_UTF8, reg,
                                    fn_diku_load, NULL, NULL,
                                    diku_registry_destroy);
    if (rc != SQLITE_OK) { free(reg); return rc; }
    rc = sqlite3_create_function(db, "diku_unload", 1, SQLITE_UTF8, reg,
                                 fn_diku_unload, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    rc = sqlite3_create_function(db, "diku_reapply_resets", 1, SQLITE_UTF8, reg,
                                 fn_diku_reapply_resets, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    rc = sqlite3_create_function(db, "diku_dir_name", 1, SQLITE_UTF8, NULL,
                                 fn_diku_dir_name, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    rc = sqlite3_create_function(db, "diku_sector_name", 1, SQLITE_UTF8, NULL,
                                 fn_diku_sector_name, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    rc = sqlite3_create_function(db, "diku_item_type_name", 1, SQLITE_UTF8, NULL,
                                 fn_diku_item_type_name, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    rc = sqlite3_create_function(db, "diku_format_name", 1, SQLITE_UTF8, NULL,
                                 fn_diku_format_name, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    rc = sqlite3_create_function(db, "diku_get_areas_ptr", 1, SQLITE_UTF8, reg,
                                 fn_diku_get_areas_ptr, NULL, NULL);
    return rc;
}

#ifndef DIKU_SQLITE_EMBED
int sqlite3_diku_init(sqlite3 *db, char **pzErrMsg,
                      const sqlite3_api_routines *pApi) {
    (void)pzErrMsg;
    SQLITE_EXTENSION_INIT2(pApi);
    return diku_sqlite_register(db);
}
/* SQLite derives the init function name from the shared-object's basename
 * by stripping non-alphanumerics: "diku_sqlite" -> sqlite3_dikusqlite_init.
 * Alias it so `.load` works regardless of the filename. */
int sqlite3_dikusqlite_init(sqlite3 *db, char **pzErrMsg,
                            const sqlite3_api_routines *pApi) {
    return sqlite3_diku_init(db, pzErrMsg, pApi);
}
#else
int sqlite3_diku_init(sqlite3 *db, char **pzErrMsg,
                      const sqlite3_api_routines *pApi) {
    (void)pzErrMsg; (void)pApi;
    return diku_sqlite_register(db);
}
#endif
