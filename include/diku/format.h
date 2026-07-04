#ifndef DIKU_FORMAT_H
#define DIKU_FORMAT_H

#include "diku/sections.h"
#include "diku/resets.h"
#include "diku/context.h"
#include "diku/schema.h"
#include <dirent.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

area_t       *diku_parse_file(diku_context_t *ctx, const char *filename);
area_t       *diku_parse_lexer(diku_lexer_t *lex, const char *filename);
area_t       *diku_parse_package(diku_context_t *ctx, const char *base_path);
area_t       *diku_parse_package_files(const char *wld, const char *mob, const char *obj, const char *zon);
area_t       *diku_load_folder_are(diku_context_t *ctx, const char *folder_path);
area_t       *diku_load_folder_packages(diku_context_t *ctx, const char *folder_path);
area_t       *diku_parse_path(diku_context_t *ctx, const char *path);
diku_format_t diku_detect_format(const area_t *area);

#ifdef DIKU_PARSER_IMPLEMENTATION

static void diku_fmt_progress(diku_context_t *ctx, const char *op, int cur, int total, const char *detail) {
    if (ctx && ctx->progress_cb)
        ctx->progress_cb(op, cur, total, detail ? detail : "", ctx->progress_user);
}

static char **diku_fmt_store_raw(diku_lexer_t *lex, memento_arena_t *arena, int *line_count) {
    char **lines = NULL;
    int count = 0;
    char buf[4096];
    while (1) {
        long line_pos = diku_lexer_tell(lex);
        if (!diku_lexer_read_line(lex, buf, sizeof(buf))) break;
        if (buf[0] == '0' && (buf[1] == '\n' || buf[1] == '\r' || buf[1] == '\0')) break;
        if (buf[0] == '#') { diku_lexer_seek(lex, line_pos); break; }
        count++;
        lines = (char **)realloc(lines, count * sizeof(char *));
        if (!lines) return NULL;
        size_t len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
        lines[count - 1] = diku_arena_strdup(arena, buf);
    }
    *line_count = count;
    return lines;
}

area_t *diku_parse_lexer(diku_lexer_t *lex, const char *filename) {
    if (!lex) return NULL;

    memento_arena_t *arena = diku_arena_create();
    if (!arena) return NULL;

    area_t *area = diku_parse_area_header(lex, arena);
    if (!area) { diku_arena_free_all(arena); return NULL; }

    area->filename = diku_arena_strndup(arena, filename, strlen(filename));
    area->format = diku_sniff_format(lex->buf, (size_t)(lex->end - lex->buf));

    char buf[256];
    while (diku_lexer_read_word(lex, buf, 255)) {
        if (buf[0] != '#') continue;
        const char *section = buf + 1;

        if (strcmp(section, "ROOMS") == 0) {
            int room_capacity = 16;
            area->rooms = (room_t *)diku_arena_alloc_aligned(arena, sizeof(room_t) * room_capacity, 64);
            if (!area->rooms) { fprintf(stderr, "ERROR: Failed to allocate rooms array\n"); continue; }
            int vnum;
            while (1) {
                diku_lexer_skip_ws(lex);
                long pos = diku_lexer_tell(lex);
                int c = diku_lexer_getc(lex);
                if (c == '#') {
                    char next[16];
                    if (diku_lexer_read_word(lex, next, 16)) {
                        if (strcmp(next, "0") == 0) break;
                        else diku_lexer_seek(lex, pos + 1);
                    }
                } else if (c == '0' || c == EOF) { break; }
                else { diku_lexer_seek(lex, pos); diku_lexer_skip_line(lex); continue; }
                room_t *room = diku_parse_room(lex, arena, &vnum);
#ifdef DIKU_VERBOSE
                if (!room) { fprintf(stderr, "diku_parse_room NULL at pos %ld\n", diku_lexer_tell(lex)); break; }
                fprintf(stderr, "ROOM #%d %s\n", vnum, room->name.str ? room->name.str : "");
#else
                if (!room) break;
#endif
                if (area->room_count >= room_capacity) {
                    room_capacity *= 2;
                    room_t *nr = (room_t *)diku_arena_alloc_aligned(arena, sizeof(room_t) * room_capacity, 64);
                    memcpy(nr, area->rooms, area->room_count * sizeof(room_t));
                    area->rooms = nr;
                }
                memcpy(&area->rooms[area->room_count++], room, sizeof(room_t));
            }

        } else if (strcmp(section, "MOBILES") == 0 || strcmp(section, "MOBILE") == 0) {
            int mob_capacity = 16;
            area->mobiles = (mobile_t *)diku_arena_alloc_aligned(arena, sizeof(mobile_t) * mob_capacity, 64);
            int vnum;
            while (1) {
                diku_lexer_skip_ws(lex);
                long pos = diku_lexer_tell(lex);
                int c = diku_lexer_getc(lex);
                if (c == '#') {
                    char next[16];
                    if (diku_lexer_read_word(lex, next, 16)) {
                        if (strcmp(next, "0") == 0) break;
                        else diku_lexer_seek(lex, pos + 1);
                    }
                } else if (c == '0' || c == EOF) { break; }
                else { diku_lexer_seek(lex, pos); diku_lexer_skip_line(lex); continue; }
                mobile_t *mob = diku_parse_mobile(lex, arena, &vnum);
                if (!mob) break;
                if (area->mobile_count >= mob_capacity) {
                    mob_capacity *= 2;
                    mobile_t *nm = (mobile_t *)diku_arena_alloc_aligned(arena, sizeof(mobile_t) * mob_capacity, 64);
                    memcpy(nm, area->mobiles, area->mobile_count * sizeof(mobile_t));
                    area->mobiles = nm;
                }
                memcpy(&area->mobiles[area->mobile_count++], mob, sizeof(mobile_t));
            }

        } else if (strcmp(section, "OBJECTS") == 0 || strcmp(section, "OBJECT") == 0 ||
                   strcmp(section, "OBJ") == 0) {
            int item_capacity = 16;
            area->items = (item_t *)diku_arena_alloc_aligned(arena, sizeof(item_t) * item_capacity, 64);
            int vnum;
            while (1) {
                diku_lexer_skip_ws(lex);
                long pos = diku_lexer_tell(lex);
                int c = diku_lexer_getc(lex);
                if (c == '#') {
                    char next[16];
                    if (diku_lexer_read_word(lex, next, 16)) {
                        if (strcmp(next, "0") == 0) break;
                        else diku_lexer_seek(lex, pos + 1);
                    }
                } else if (c == '0' || c == EOF) { break; }
                else { diku_lexer_seek(lex, pos); diku_lexer_skip_line(lex); continue; }
                item_t *item = diku_parse_item(lex, arena, &vnum);
                if (!item) break;
                if (area->item_count >= item_capacity) {
                    item_capacity *= 2;
                    item_t *ni = (item_t *)diku_arena_alloc_aligned(arena, sizeof(item_t) * item_capacity, 64);
                    memcpy(ni, area->items, area->item_count * sizeof(item_t));
                    area->items = ni;
                }
                memcpy(&area->items[area->item_count++], item, sizeof(item_t));
            }

        } else if (strcmp(section, "HELPS") == 0) {
            area->helps_raw = diku_fmt_store_raw(lex, arena, &area->helps_line_count);
        } else if (strcmp(section, "RESETS") == 0 || strcmp(section, "RESET") == 0) {
            area->resets_raw = diku_fmt_store_raw(lex, arena, &area->resets_line_count);
        } else if (strcmp(section, "SHOPS") == 0 || strcmp(section, "SHOP") == 0) {
            area->shops_raw = diku_fmt_store_raw(lex, arena, &area->shops_line_count);
        } else if (strcmp(section, "SPECIALS") == 0 || strcmp(section, "SPECIAL") == 0) {
            area->specials_raw = diku_fmt_store_raw(lex, arena, &area->specials_line_count);
        } else if (strcmp(section, "OBJFUNS") == 0 || strcmp(section, "OBJFUN") == 0) {
            area->objfuns_raw = diku_fmt_store_raw(lex, arena, &area->objfuns_line_count);
        } else if (strcmp(section, "$") == 0) {
            break;
        } else {
            area->extra_section_count++;
            diku_fmt_store_raw(lex, arena, &(int){0});
        }
    }

    diku_build_vnum_hash(area);
    diku_parse_resets(area);

    diku_format_t classified = diku_classify_area(area);
    if (diku_format_priority(classified) > diku_format_priority(area->format))
        area->format = classified;

    return area;
}

area_t *diku_parse_file(diku_context_t *ctx, const char *filename) {
    (void)ctx;
    diku_lexer_t lex;
    if (diku_lexer_init_file(&lex, filename)) {
        area_t *area = diku_parse_lexer(&lex, filename);
        diku_lexer_cleanup(&lex);
        if (area) return area;
    }
    return diku_parse_package(ctx, filename);
}

area_t *diku_parse_package_files(const char *wld, const char *mob, const char *obj, const char *zon) {
    memento_arena_t *arena = diku_arena_create();
    if (!arena) return NULL;

    area_t *area = (area_t *)diku_arena_alloc_aligned(arena, sizeof(area_t), 64);
    if (!area) { diku_arena_free_all(arena); return NULL; }
    memset(area, 0, sizeof(area_t));
    area->arena = arena;

    bool has_data = false;
    { diku_lexer_t fl; if (diku_lexer_init_file(&fl, wld)) { area->filename = diku_arena_strndup(arena, wld, strlen(wld)); if (diku_parse_wld(&fl, area)) has_data = true; diku_lexer_cleanup(&fl); } }
    { diku_lexer_t fl; if (diku_lexer_init_file(&fl, mob)) { if (diku_parse_mob(&fl, area)) has_data = true; diku_lexer_cleanup(&fl); } }
    { diku_lexer_t fl; if (diku_lexer_init_file(&fl, obj)) { if (diku_parse_obj(&fl, area)) has_data = true; diku_lexer_cleanup(&fl); } }
    { diku_lexer_t fl; if (diku_lexer_init_file(&fl, zon)) { if (diku_parse_zon(&fl, area)) has_data = true; diku_lexer_cleanup(&fl); } }

    if (!has_data) { diku_free_area(area); return NULL; }
    area->format = DIKU_FMT_CIRCLE;
    diku_build_vnum_hash(area);
    return area;
}

area_t *diku_parse_package(diku_context_t *ctx, const char *base_path) {
    (void)ctx;
    size_t len = strlen(base_path);
    char *wld = (char *)malloc(len + 5);
    char *mob = (char *)malloc(len + 5);
    char *obj = (char *)malloc(len + 5);
    char *zon = (char *)malloc(len + 5);
    if (!wld || !mob || !obj || !zon) { free(wld); free(mob); free(obj); free(zon); return NULL; }
    sprintf(wld, "%s.wld", base_path);
    sprintf(mob, "%s.mob", base_path);
    sprintf(obj, "%s.obj", base_path);
    sprintf(zon, "%s.zon", base_path);
    area_t *area = diku_parse_package_files(wld, mob, obj, zon);
    free(wld); free(mob); free(obj); free(zon);
    return area;
}

area_t *diku_load_folder_are(diku_context_t *ctx, const char *folder_path) {
    DIR *dir = opendir(folder_path);
    if (!dir) return NULL;
    struct dirent *entry;
    int total = 0;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcasecmp(entry->d_name + len - 4, ".are") == 0) total++;
    }
    rewinddir(dir);
    area_t *head = NULL, *tail = NULL;
    int current = 0;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len <= 4 || strcasecmp(entry->d_name + len - 4, ".are") != 0) continue;
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", folder_path, entry->d_name);
        diku_fmt_progress(ctx, "parse_are", current, total, path);
        area_t *area = diku_parse_file(ctx, path);
        if (area) { if (!head) head = tail = area; else { tail->next = area; tail = area; } }
        current++;
    }
    closedir(dir);
    diku_fmt_progress(ctx, "parse_are", total, total, "done");
    return head;
}

area_t *diku_load_folder_packages(diku_context_t *ctx, const char *folder_path) {
    DIR *dir = opendir(folder_path);
    if (!dir) return NULL;
    struct dirent *entry;
    int total = 0;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcasecmp(entry->d_name + len - 4, ".wld") == 0) total++;
    }
    rewinddir(dir);
    area_t *head = NULL, *tail = NULL;
    int current = 0;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len <= 4 || strcasecmp(entry->d_name + len - 4, ".wld") != 0) continue;
        char base[4096];
        snprintf(base, sizeof(base), "%s/%.*s", folder_path, (int)(len - 4), entry->d_name);
        diku_fmt_progress(ctx, "parse_package", current, total, base);
        area_t *area = diku_parse_package(ctx, base);
        if (area) { if (!head) head = tail = area; else { tail->next = area; tail = area; } }
        current++;
    }
    closedir(dir);
    diku_fmt_progress(ctx, "parse_package", total, total, "done");
    return head;
}

area_t *diku_parse_path(diku_context_t *ctx, const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return NULL;
    if (S_ISREG(st.st_mode)) {
        return diku_parse_file(ctx, path);
    } else if (S_ISDIR(st.st_mode)) {
        return diku_load_folder_are(ctx, path);
    }
    return NULL;
}

diku_format_t diku_detect_format(const area_t *area) {
    if (!area) return DIKU_FMT_UNKNOWN;
    /* If the parser already determined a fork, use it. */
    if (area->format != DIKU_FMT_UNKNOWN) return area->format;
    /* Fallback heuristic for areas created outside the parser. */
    for (int i = 0; i < area->mobile_count; i++)
        if (area->mobiles[i].material.len > 0) return DIKU_FMT_ROM;
    for (int i = 0; i < area->item_count; i++)
        if (area->items[i].material.len > 0) return DIKU_FMT_ROM;
    if (area->security > 0 || area->version > 0) return DIKU_FMT_SMAUG;
    if (area->room_count > 0 && area->mobile_count > 0) return DIKU_FMT_MERC;
    return DIKU_FMT_DIKU;
}

#endif /* DIKU_PARSER_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif
#endif /* DIKU_FORMAT_H */
