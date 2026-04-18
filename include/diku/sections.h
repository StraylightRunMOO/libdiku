#ifndef DIKU_SECTIONS_H
#define DIKU_SECTIONS_H

#include "diku/lexer.h"
#include "diku/find.h"

#ifdef __cplusplus
extern "C" {
#endif

area_t   *diku_parse_area_header(diku_lexer_t *lex, memento_arena_t *arena);
room_t   *diku_parse_room(diku_lexer_t *lex, memento_arena_t *arena, int *vnum_out);
mobile_t *diku_parse_mobile(diku_lexer_t *lex, memento_arena_t *arena, int *vnum_out);
item_t   *diku_parse_item(diku_lexer_t *lex, memento_arena_t *arena, int *vnum_out);
bool      diku_parse_wld(diku_lexer_t *lex, area_t *area);
bool      diku_parse_mob(diku_lexer_t *lex, area_t *area);
bool      diku_parse_obj(diku_lexer_t *lex, area_t *area);
bool      diku_parse_zon(diku_lexer_t *lex, area_t *area);

#ifdef DIKU_PARSER_IMPLEMENTATION

static bool parse_header_line(area_t *area, diku_lexer_t *lex, char letter, memento_arena_t *arena) {
    switch (letter) {
        case 'K': area->builders          = diku_lexer_read_string(lex, arena);     return true;
        case 'U': area->ambient_sound      = diku_lexer_read_string(lex, arena);     return true;
        case 'O': area->owner             = diku_lexer_read_string(lex, arena);     return true;
        case 'R': area->reset_msg         = diku_lexer_read_string(lex, arena);     return true;
        case 'W': area->weather           = diku_lexer_read_string(lex, arena);     return true;
        case 'P': area->pay_info          = diku_lexer_read_string_eol(lex, arena); return true;
        case 'T': area->teleport_info     = diku_lexer_read_string_eol(lex, arena); return true;
        case 'M': area->magic_info        = diku_lexer_read_string_eol(lex, arena); return true;
        default:  diku_lexer_skip_line(lex); return true;
    }
}

area_t *diku_parse_area_header(diku_lexer_t *lex, memento_arena_t *arena) {
    area_t *area = (area_t *)diku_arena_alloc_aligned(arena, sizeof(area_t), 64);
    if (!area) return NULL;
    memset(area, 0, sizeof(area_t));
    area->arena = arena;

    char buf[256];
    if (!diku_lexer_read_line(lex, buf, sizeof(buf))) return NULL;

    if (strstr(buf, "AREA") || strstr(buf, "#AREA")) {
        char *name_start = strchr(buf, ' ');
        if (name_start) {
            name_start++;
            char *tilde = strchr(name_start, '~');
            if (tilde) {
                *tilde = '\0';
                size_t len = strlen(name_start);
                while (len > 0 && (name_start[len-1] == '\n' || name_start[len-1] == '\r'))
                    name_start[--len] = '\0';
                area->name = diku_arena_strndup(arena, name_start, len);
            }
        }
        if (!area->name.str || area->name.len == 0)
            area->name = diku_lexer_read_string(lex, arena);
    }

    int c;
    while ((c = diku_lexer_getc(lex)) != EOF) {
        if (c == '*') { diku_lexer_skip_line(lex); continue; }
        if (c == '#') {
            long hash_pos = diku_lexer_tell(lex) - 1;
            char peek[16];
            if (diku_lexer_read_word(lex, peek, 16)) {
                if (strcmp(peek, "ROOMS") == 0 || strcmp(peek, "MOBILES") == 0 ||
                    strcmp(peek, "OBJECTS") == 0 || strcmp(peek, "HELPS") == 0 ||
                    strcmp(peek, "RESETS") == 0 || strcmp(peek, "SHOPS") == 0 ||
                    strcmp(peek, "SPECIALS") == 0 || strcmp(peek, "OBJFUNS") == 0 ||
                    strcmp(peek, "$") == 0 || isdigit(peek[0])) {
                    diku_lexer_seek(lex, hash_pos);
                    break;
                } else {
                    diku_lexer_skip_line(lex);
                    continue;
                }
            }
        }
        if (isspace(c)) continue;
        if (c < 'A' || c > 'Z') { diku_lexer_skip_line(lex); continue; }
        parse_header_line(area, lex, (char)c, arena);
    }
    return area;
}

static exit_t *parse_exit(diku_lexer_t *lex, memento_arena_t *arena, int direction) {
    exit_t *exit = (exit_t *)diku_arena_alloc(arena, sizeof(exit_t));
    if (!exit) return NULL;
    memset(exit, 0, sizeof(exit_t));
    exit->direction = direction;
    exit->key_vnum  = -1;
    exit->to_vnum   = -1;

    exit->desc     = diku_lexer_read_string(lex, arena);
    exit->keywords = diku_lexer_read_string(lex, arena);

    diku_lexer_skip_ws(lex);
    int lock, key, to_vnum;
    if (diku_lexer_read_number(lex, &lock) == 0) {
        exit->flags = lock;
        diku_lexer_read_number(lex, &key);
        exit->key_vnum = key;
        diku_lexer_read_number(lex, &to_vnum);
        exit->to_vnum = to_vnum;
    }
    return exit;
}

room_t *diku_parse_room(diku_lexer_t *lex, memento_arena_t *arena, int *vnum_out) {
    int vnum;
    if (diku_lexer_read_number(lex, &vnum) != 0) return NULL;
    *vnum_out = vnum;

    room_t *room = (room_t *)diku_arena_alloc_aligned(arena, sizeof(room_t), 64);
    if (!room) return NULL;
    memset(room, 0, sizeof(room_t));
    room->vnum = vnum;
    room->coord_assigned = false;

    room->name = diku_lexer_read_string(lex, arena);
    room->desc = diku_lexer_read_string(lex, arena);

    diku_lexer_skip_ws(lex);
    long flags_pos = diku_lexer_tell(lex);
    int peek = diku_lexer_getc(lex);
    if (peek != EOF) {
        if (peek == 'D' || peek == 'E' || peek == 'S' || peek == '~' || peek == '#' || peek == '*') {
            diku_lexer_ungetc(lex, peek);
        } else {
            diku_lexer_seek(lex, flags_pos);
            char token1[64], token2[64];
            if (diku_lexer_read_word(lex, token1, 64)) {
                long after_t1 = diku_lexer_tell(lex);
                int c2;
                while ((c2 = diku_lexer_getc(lex)) != EOF && isspace(c2)) {}
                if (c2 != EOF) {
                    diku_lexer_ungetc(lex, c2);
                    if (isalpha((unsigned char)c2)) {
                        if (diku_lexer_read_word(lex, token2, 64)) {
                            uint32_t flags = 0;
                            for (int i = 0; token2[i]; i++) {
                                if (token2[i] >= 'A' && token2[i] <= 'Z')      flags |= (1U << (token2[i] - 'A'));
                                else if (token2[i] >= 'a' && token2[i] <= 'z') flags |= (1U << (26 + token2[i] - 'a'));
                            }
                            room->flags = flags;
                            diku_lexer_read_number(lex, &room->sector);
                        }
                    } else {
                        diku_lexer_seek(lex, after_t1);
                        if (isdigit(token1[0]) || token1[0] == '-') {
                            room->flags = (uint32_t)strtol(token1, NULL, 0);
                        } else {
                            uint32_t flags = 0;
                            for (int i = 0; token1[i]; i++) {
                                if (token1[i] >= 'A' && token1[i] <= 'Z')      flags |= (1U << (token1[i] - 'A'));
                                else if (token1[i] >= 'a' && token1[i] <= 'z') flags |= (1U << (26 + token1[i] - 'a'));
                            }
                            room->flags = flags;
                        }
                        diku_lexer_read_number(lex, &room->sector);
                    }
                }
            }
        }
    }

    int c;
    while ((c = diku_lexer_getc(lex)) != EOF) {
        if (c == '*') { diku_lexer_skip_line(lex); continue; }
        if (isspace(c)) continue;
        if (c == 'S' || c == 's') {
            int next = diku_lexer_getc(lex);
            if (next == '\n' || next == '\r' || next == EOF) break;
            if (next != EOF) diku_lexer_ungetc(lex, next);
        }
        if (c == 'D' || c == 'd') {
            int dir;
            if (diku_lexer_read_number(lex, &dir) == 0 && dir >= 0 && dir < DIKU_MAX_EXITS) {
                room->exits[dir] = parse_exit(lex, arena, dir);
            } else {
                diku_lexer_read_string(lex, arena);
                diku_lexer_read_string(lex, arena);
                diku_lexer_skip_line(lex);
            }
            continue;
        }
        if (c == 'E') {
            int idx = room->extra_desc_count++;
            size_t new_size = room->extra_desc_count * sizeof(*room->extra_descs);
            if (room->extra_desc_count == 1) {
                room->extra_descs = diku_arena_alloc(arena, new_size);
            } else {
                void *new_array = diku_arena_alloc(arena, new_size);
                if (new_array && room->extra_descs) {
                    memcpy(new_array, room->extra_descs, (room->extra_desc_count - 1) * sizeof(*room->extra_descs));
                    room->extra_descs = new_array;
                }
            }
            if (room->extra_descs) {
                room->extra_descs[idx].keywords = diku_lexer_read_string(lex, arena);
                room->extra_descs[idx].desc     = diku_lexer_read_string(lex, arena);
            }
            continue;
        }
        diku_lexer_skip_line(lex);
    }
    return room;
}

mobile_t *diku_parse_mobile(diku_lexer_t *lex, memento_arena_t *arena, int *vnum_out) {
    int vnum;
    if (diku_lexer_read_number(lex, &vnum) != 0) return NULL;
    *vnum_out = vnum;

    mobile_t *mob = (mobile_t *)diku_arena_alloc_aligned(arena, sizeof(mobile_t), 64);
    if (!mob) return NULL;
    memset(mob, 0, sizeof(mobile_t));
    mob->vnum = vnum;

    mob->name       = diku_lexer_read_string(lex, arena);
    mob->short_desc = diku_lexer_read_string(lex, arena);
    mob->long_desc  = diku_lexer_read_string(lex, arena);
    diku_string_t desc = diku_lexer_read_string(lex, arena);
    if (desc.len > 0) mob->description = desc;

    diku_lexer_skip_ws(lex);
    char buf[256];

    if (diku_lexer_read_word(lex, buf, 255)) {
        uint32_t flags = 0;
        if (isdigit(buf[0]) || buf[0] == '-') { flags = (uint32_t)strtol(buf, NULL, 0); }
        else { for (int i = 0; buf[i]; i++) { if (buf[i] >= 'A' && buf[i] <= 'Z') flags |= (1U << (buf[i]-'A')); else if (buf[i] >= 'a' && buf[i] <= 'z') flags |= (1U << (26+buf[i]-'a')); } }
        mob->act_flags = flags;
    }
    if (diku_lexer_read_word(lex, buf, 255)) {
        uint32_t flags = 0;
        if (isdigit(buf[0]) || buf[0] == '-') { flags = (uint32_t)strtol(buf, NULL, 0); }
        else { for (int i = 0; buf[i]; i++) { if (buf[i] >= 'A' && buf[i] <= 'Z') flags |= (1U << (buf[i]-'A')); else if (buf[i] >= 'a' && buf[i] <= 'z') flags |= (1U << (26+buf[i]-'a')); } }
        mob->aff_flags = flags;
    }

    diku_lexer_read_number(lex, (int *)&mob->alignment);

    char format_letter = 'S';
    diku_lexer_skip_ws(lex);
    int c = diku_lexer_getc(lex);
    if (c != EOF && isalpha(c)) format_letter = (char)c;
    else if (c != EOF) diku_lexer_ungetc(lex, c);
    diku_lexer_skip_line(lex);

    if (format_letter == 'S') {
        diku_lexer_read_number(lex, &mob->level);
        diku_lexer_read_number(lex, &mob->hitroll);
        int ac, hp_num, hp_type, hp_bonus;
        diku_lexer_read_number(lex, &ac);
        mob->ac[0] = mob->ac[1] = mob->ac[2] = mob->ac[3] = ac;
        diku_lexer_read_number(lex, &hp_num);
        diku_lexer_read_number(lex, &hp_type);
        diku_lexer_read_number(lex, &hp_bonus);
        mob->hit[0] = hp_num; mob->hit[1] = hp_type; mob->hit[2] = hp_bonus;
        diku_lexer_skip_line(lex);
    } else if (format_letter == 'C' || format_letter == 'c') {
        diku_lexer_read_number(lex, &mob->level);
        diku_lexer_read_number(lex, &mob->hitroll);
        diku_lexer_read_number(lex, &mob->hit[0]); diku_lexer_read_number(lex, &mob->hit[1]); diku_lexer_read_number(lex, &mob->hit[2]);
        diku_lexer_read_number(lex, &mob->mana[0]); diku_lexer_read_number(lex, &mob->mana[1]); diku_lexer_read_number(lex, &mob->mana[2]);
        diku_lexer_read_number(lex, &mob->damage[0]); diku_lexer_read_number(lex, &mob->damage[1]); diku_lexer_read_number(lex, &mob->damage[2]);
        diku_lexer_skip_line(lex);
        diku_lexer_read_number(lex, &mob->ac[0]); diku_lexer_read_number(lex, &mob->ac[1]); diku_lexer_read_number(lex, &mob->ac[2]); diku_lexer_read_number(lex, &mob->ac[3]);
        diku_lexer_skip_line(lex);
        diku_lexer_skip_ws(lex);
        if (diku_lexer_read_word(lex, buf, 255)) mob->off_flags  = (uint32_t)strtol(buf, NULL, 0);
        if (diku_lexer_read_word(lex, buf, 255)) mob->imm_flags  = (uint32_t)strtol(buf, NULL, 0);
        if (diku_lexer_read_word(lex, buf, 255)) mob->res_flags  = (uint32_t)strtol(buf, NULL, 0);
        if (diku_lexer_read_word(lex, buf, 255)) mob->vuln_flags = (uint32_t)strtol(buf, NULL, 0);
        diku_lexer_skip_line(lex);
        diku_lexer_read_number(lex, &mob->start_pos); diku_lexer_read_number(lex, &mob->default_pos); diku_lexer_read_number(lex, &mob->sex);
        diku_lexer_skip_ws(lex);
        if (diku_lexer_read_word(lex, buf, 255) && isdigit(buf[0])) mob->gold = strtol(buf, NULL, 0);
        diku_lexer_skip_line(lex);
    } else {
        diku_lexer_skip_line(lex);
    }

    long pos = diku_lexer_tell(lex);
    int next_c;
    while ((next_c = diku_lexer_getc(lex)) != EOF) {
        if (next_c == '#' || (next_c == '0' && (next_c = diku_lexer_getc(lex)) == '\n')) {
            diku_lexer_seek(lex, pos); break;
        }
        if (next_c == 'M') mob->material = diku_lexer_read_string(lex, arena);
        else if (next_c != '\n' && next_c != '\r') diku_lexer_skip_line(lex);
        pos = diku_lexer_tell(lex);
    }
    return mob;
}

item_t *diku_parse_item(diku_lexer_t *lex, memento_arena_t *arena, int *vnum_out) {
    int vnum;
    if (diku_lexer_read_number(lex, &vnum) != 0) return NULL;
    *vnum_out = vnum;

    item_t *item = (item_t *)diku_arena_alloc_aligned(arena, sizeof(item_t), 64);
    if (!item) return NULL;
    memset(item, 0, sizeof(item_t));
    item->vnum = vnum;

    item->name       = diku_lexer_read_string(lex, arena);
    item->short_desc = diku_lexer_read_string(lex, arena);
    item->long_desc  = diku_lexer_read_string(lex, arena);

    diku_lexer_skip_ws(lex);
    int desc_peek = diku_lexer_getc(lex);
    if (desc_peek != EOF) {
        if (desc_peek == '~') { /* empty */ }
        else if (isdigit(desc_peek) || desc_peek == '#' || desc_peek == '0') { diku_lexer_ungetc(lex, desc_peek); }
        else {
            diku_lexer_ungetc(lex, desc_peek);
            diku_string_t desc = diku_lexer_read_string(lex, arena);
            if (desc.len > 0) item->description = desc;
        }
    }

    diku_lexer_skip_ws(lex);
    long boundary_pos = diku_lexer_tell(lex);
    int boundary_c = diku_lexer_getc(lex);
    if (boundary_c == '#') {
        int boundary_next = diku_lexer_getc(lex);
        if (boundary_next == '0') { diku_lexer_seek(lex, boundary_pos); return item; }
        else if (boundary_next != EOF) diku_lexer_ungetc(lex, boundary_next);
    }
    if (boundary_c != EOF) diku_lexer_ungetc(lex, boundary_c);

    diku_lexer_skip_ws(lex);
    diku_lexer_read_number(lex, &item->type);

    char buf[256];
    if (diku_lexer_read_word(lex, buf, 255)) {
        uint32_t flags = 0;
        if (isdigit(buf[0]) || buf[0] == '-') { flags = (uint32_t)strtol(buf, NULL, 0); }
        else { for (int i = 0; buf[i]; i++) { if (buf[i] >= 'A' && buf[i] <= 'Z') flags |= (1U << (buf[i]-'A')); else if (buf[i] >= 'a' && buf[i] <= 'z') flags |= (1U << (26+buf[i]-'a')); } }
        item->extra_flags = flags;
    }
    if (diku_lexer_read_word(lex, buf, 255)) {
        uint32_t flags = 0;
        if (isdigit(buf[0]) || buf[0] == '-') { flags = (uint32_t)strtol(buf, NULL, 0); }
        else { for (int i = 0; buf[i]; i++) { if (buf[i] >= 'A' && buf[i] <= 'Z') flags |= (1U << (buf[i]-'A')); else if (buf[i] >= 'a' && buf[i] <= 'z') flags |= (1U << (26+buf[i]-'a')); } }
        item->wear_flags = flags;
    }
    diku_lexer_skip_line(lex);

    diku_lexer_skip_ws(lex);
    for (int i = 0; i < 4; i++) diku_lexer_read_number(lex, &item->value[i]);
    diku_lexer_skip_line(lex);

    diku_lexer_skip_ws(lex);
    diku_lexer_read_number(lex, &item->weight);
    diku_lexer_read_number(lex, &item->cost);
    long pos = diku_lexer_tell(lex);
    int level;
    if (diku_lexer_read_number(lex, &level) == 0) item->level = level;
    else diku_lexer_seek(lex, pos);
    diku_lexer_skip_line(lex);

    pos = diku_lexer_tell(lex);
    int next_c;
    while ((next_c = diku_lexer_getc(lex)) != EOF) {
        if (next_c == '#' || (next_c == '0' && (next_c = diku_lexer_getc(lex)) == '\n')) {
            diku_lexer_seek(lex, pos); break;
        }
        if (next_c == 'A') {
            int idx = item->affect_count++;
            void *na = diku_arena_alloc(arena, item->affect_count * sizeof(*item->affects));
            if (na && item->affects) memcpy(na, item->affects, idx * sizeof(*item->affects));
            item->affects = na;
            if (item->affects) { diku_lexer_read_number(lex, &item->affects[idx].location); diku_lexer_read_number(lex, &item->affects[idx].modifier); }
            diku_lexer_skip_line(lex);
        } else if (next_c == 'E') {
            int idx = item->extra_desc_count++;
            void *na = diku_arena_alloc(arena, item->extra_desc_count * sizeof(*item->extra_descs));
            if (na && item->extra_descs) memcpy(na, item->extra_descs, idx * sizeof(*item->extra_descs));
            item->extra_descs = na;
            if (item->extra_descs) { item->extra_descs[idx].keywords = diku_lexer_read_string(lex, arena); item->extra_descs[idx].desc = diku_lexer_read_string(lex, arena); }
        } else if (next_c == 'L') {
            diku_lexer_read_number(lex, &item->level); diku_lexer_skip_line(lex);
        } else if (next_c == 'M') {
            item->material = diku_lexer_read_string(lex, arena);
        } else if (next_c != '\n' && next_c != '\r') {
            diku_lexer_skip_line(lex);
        }
        pos = diku_lexer_tell(lex);
    }
    return item;
}

static bool diku_parse_section_entities(diku_lexer_t *lex, area_t *area, int entity_type) {
    int capacity = 16;
    int *count_ptr = NULL;
    void **array_ptr = NULL;
    size_t elem_size = 0;

    if (entity_type == 0) { /* rooms */
        if (area->room_count == 0) area->rooms = (room_t *)diku_arena_alloc_aligned(area->arena, sizeof(room_t) * capacity, 64);
        count_ptr = &area->room_count; array_ptr = (void **)&area->rooms; elem_size = sizeof(room_t);
    } else if (entity_type == 1) { /* mobiles */
        if (area->mobile_count == 0) area->mobiles = (mobile_t *)diku_arena_alloc_aligned(area->arena, sizeof(mobile_t) * capacity, 64);
        count_ptr = &area->mobile_count; array_ptr = (void **)&area->mobiles; elem_size = sizeof(mobile_t);
    } else {
        if (area->item_count == 0) area->items = (item_t *)diku_arena_alloc_aligned(area->arena, sizeof(item_t) * capacity, 64);
        count_ptr = &area->item_count; array_ptr = (void **)&area->items; elem_size = sizeof(item_t);
    }

    int parsed = 0;
    while (1) {
        diku_lexer_skip_ws(lex);
        int c = diku_lexer_getc(lex);
        if (c == EOF || c == '$') break;
        if (c == '#') {
            int next = diku_lexer_getc(lex);
            if (next == '$' || next == '0') break;
            if (isdigit(next)) {
                diku_lexer_ungetc(lex, next);
                int vnum;
                void *entity = NULL;
                if (entity_type == 0) entity = diku_parse_room(lex, area->arena, &vnum);
                else if (entity_type == 1) entity = diku_parse_mobile(lex, area->arena, &vnum);
                else entity = diku_parse_item(lex, area->arena, &vnum);
                if (!entity) break;
                if (*count_ptr >= capacity) {
                    capacity *= 2;
                    void *new_array = diku_arena_alloc_aligned(area->arena, elem_size * capacity, 64);
                    if (new_array && *array_ptr) { memcpy(new_array, *array_ptr, *count_ptr * elem_size); *array_ptr = new_array; }
                }
                if (*array_ptr) { memcpy((char *)*array_ptr + (*count_ptr) * elem_size, entity, elem_size); (*count_ptr)++; }
                parsed++;
                continue;
            }
            if (next != EOF) diku_lexer_ungetc(lex, next);
        }
        if (c != EOF) diku_lexer_ungetc(lex, c);
        diku_lexer_skip_line(lex);
    }
    return parsed > 0;
}

bool diku_parse_wld(diku_lexer_t *lex, area_t *area) { return diku_parse_section_entities(lex, area, 0); }
bool diku_parse_mob(diku_lexer_t *lex, area_t *area) { return diku_parse_section_entities(lex, area, 1); }
bool diku_parse_obj(diku_lexer_t *lex, area_t *area) { return diku_parse_section_entities(lex, area, 2); }

bool diku_parse_zon(diku_lexer_t *lex, area_t *area) {
    if (!lex || !area) return false;
    diku_lexer_skip_ws(lex);
    int c = diku_lexer_getc(lex);
    if (c == '#') { int zone_num; diku_lexer_read_number(lex, &zone_num); diku_lexer_skip_ws(lex); c = diku_lexer_getc(lex); }
    if (c != EOF) diku_lexer_ungetc(lex, c);

    diku_string_t name = diku_lexer_read_string(lex, area->arena);
    if (name.len > 0 && (!area->name.str || area->name.len == 0)) area->name = name;
    diku_lexer_skip_line(lex);

    char **lines = NULL; int count = 0; char buf[4096];
    while (diku_lexer_read_line(lex, buf, sizeof(buf))) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
        if (len == 0 || buf[0] == '*') continue;
        if (buf[0] == 'S' || buf[0] == 's' || buf[0] == '$') break;
        count++;
        char **new_lines = (char **)diku_arena_alloc(area->arena, count * sizeof(char *));
        if (new_lines && lines) memcpy(new_lines, lines, (count - 1) * sizeof(char *));
        lines = new_lines;
        if (lines) lines[count - 1] = diku_arena_strdup(area->arena, buf);
    }
    area->resets_raw = lines;
    area->resets_line_count = count;
    diku_parse_resets(area);
    return true;
}

#endif /* DIKU_PARSER_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif
#endif /* DIKU_SECTIONS_H */
