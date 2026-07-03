#ifndef DIKU_SECTIONS_H
#define DIKU_SECTIONS_H

#include "diku/lexer.h"
#include "diku/find.h"
#include "diku/schema.h"

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

static bool is_area_section_word(const char *word) {
    if (!word || !*word) return false;
    if (strcmp(word, "$") == 0) return true;
    if (isdigit((unsigned char)word[0])) return true;
    return strcmp(word, "ROOMS") == 0 || strcmp(word, "ROOM") == 0 ||
           strcmp(word, "MOBILES") == 0 || strcmp(word, "MOBILE") == 0 ||
           strcmp(word, "OBJECTS") == 0 || strcmp(word, "OBJECT") == 0 ||
           strcmp(word, "OBJ") == 0 || strcmp(word, "HELPS") == 0 ||
           strcmp(word, "RESETS") == 0 || strcmp(word, "RESET") == 0 ||
           strcmp(word, "SHOPS") == 0 || strcmp(word, "SHOP") == 0 ||
           strcmp(word, "SPECIALS") == 0 || strcmp(word, "SPECIAL") == 0 ||
           strcmp(word, "OBJFUNS") == 0 || strcmp(word, "OBJFUN") == 0 ||
           strcmp(word, "PROGS") == 0 || strcmp(word, "REPAIRS") == 0;
}

static diku_string_t diku_extract_tilded_name(memento_arena_t *arena, const char *line) {
    diku_string_t result = {NULL, 0};
    const char *start = line;
    while (*start && isspace((unsigned char)*start)) start++;
    const char *tilde = strchr(start, '~');
    if (!tilde) return result;
    size_t len = (size_t)(tilde - start);
    while (len > 0 && (start[len - 1] == '\r' || start[len - 1] == '\n' ||
                       start[len - 1] == ' '  || start[len - 1] == '\t'))
        len--;
    return diku_arena_strndup(arena, start, len);
}

/* For compact ROM/Merc headers like "{low high} Author   Area Name~" the area
   name follows the (single-word) author. Skip the author if more tokens follow. */
static diku_string_t diku_extract_area_name_from_credits(memento_arena_t *arena, const char *line) {
    diku_string_t full = diku_extract_tilded_name(arena, line);
    if (!full.str || full.len == 0) return full;
    /* Count whitespace-separated tokens. */
    int tokens = 0;
    const char *p = full.str;
    const char *end = full.str + full.len;
    while (p < end) {
        while (p < end && isspace((unsigned char)*p)) p++;
        if (p >= end) break;
        tokens++;
        while (p < end && !isspace((unsigned char)*p)) p++;
    }
    if (tokens <= 1) return full;
    /* Skip the first token (author) and return the rest as area name. */
    p = full.str;
    while (p < end && isspace((unsigned char)*p)) p++;
    while (p < end && !isspace((unsigned char)*p)) p++;
    while (p < end && isspace((unsigned char)*p)) p++;
    size_t len = (size_t)(end - p);
    while (len > 0 && isspace((unsigned char)p[len - 1])) len--;
    return diku_arena_strndup(arena, p, len);
}

static void parse_smaug_header(area_t *area, diku_lexer_t *lex, memento_arena_t *arena) {
    char buf[256];
    while (diku_lexer_read_line(lex, buf, sizeof(buf))) {
        char *p = buf;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '*') continue;
        if (strncmp(p, "End", 3) == 0) break;

        char key[64];
        int n = 0;
        while (*p && !isspace((unsigned char)*p) && n < 63) key[n++] = *p++;
        key[n] = '\0';
        if (n == 0) continue;
        while (*p && isspace((unsigned char)*p)) p++;

        if (strcmp(key, "Name") == 0) {
            area->name = diku_extract_tilded_name(arena, p);
        } else if (strcmp(key, "Builders") == 0) {
            area->builders = diku_extract_tilded_name(arena, p);
        } else if (strcmp(key, "VNUMs") == 0 || strcmp(key, "Vnums") == 0) {
            int lo, hi;
            if (sscanf(p, "%d %d", &lo, &hi) == 2) {
                area->low_vnum = lo;
                area->high_vnum = hi;
            }
        } else if (strcmp(key, "Credits") == 0) {
            area->credits = diku_extract_tilded_name(arena, p);
            if (area->credits.str) {
                char *b = strchr(area->credits.str, '{');
                if (b) sscanf(b + 1, "%d %d", &area->low_level, &area->high_level);
            }
        } else if (strcmp(key, "Security") == 0) {
            area->security = atoi(p);
        } else if (strcmp(key, "Version") == 0) {
            area->version = atoi(p);
        } else if (strcmp(key, "Duration") == 0) {
            area->reset_interval = atoi(p);
        } else if (strcmp(key, "Ranges") == 0) {
            int lo_lvl, hi_lvl;
            if (sscanf(p, "%d %d %*d %*d", &lo_lvl, &hi_lvl) >= 2) {
                area->low_level = lo_lvl;
                area->high_level = hi_lvl;
            }
        } else if (strcmp(key, "Levels") == 0) {
            sscanf(p, "%d %d", &area->low_level, &area->high_level);
        }
    }
}

static bool parse_rom_level_range(const char *line, int *low, int *high) {
    const char *b = strchr(line, '{');
    if (!b) return false;
    return sscanf(b + 1, "%d %d", low, high) == 2;
}

area_t *diku_parse_area_header(diku_lexer_t *lex, memento_arena_t *arena) {
    area_t *area = (area_t *)diku_arena_alloc_aligned(arena, sizeof(area_t), 64);
    if (!area) return NULL;
    memset(area, 0, sizeof(area_t));
    area->arena = arena;

    char buf[256];
    if (!diku_lexer_read_line(lex, buf, sizeof(buf))) return NULL;

    /* SMAUG keyword header. */
    if (strstr(buf, "#AREADATA")) {
        
        parse_smaug_header(area, lex, arena);
    }
    /* Standard #AREA header (Diku/Merc/ROM variants). */
    else if (strstr(buf, "#AREA") || strstr(buf, "AREA")) {
        char *brace = strchr(buf, '{');
        if (brace) {
            /* Compact ROM header: #AREA {low high} credits~ */
            
            parse_rom_level_range(buf, &area->low_level, &area->high_level);
            char *close = strchr(brace, '}');
            area->name = diku_extract_area_name_from_credits(arena, close ? close + 1 : brace + 1);
        } else {
            /* Inline area name on the #AREA line, e.g. "#AREA   Asgard~". */
            char *name_start = strchr(buf, ' ');
            if (name_start) {
                name_start++;
                while (*name_start && isspace((unsigned char)*name_start)) name_start++;
                area->name = diku_extract_tilded_name(arena, name_start);
            }
        }

        if (!area->name.str || area->name.len == 0) {
            /* Read the file-name line (e.g. "haon.are~") and discard it. */
            diku_lexer_read_string(lex, arena);
            /* Read the real area name. */
            area->name = diku_lexer_read_string(lex, arena);
        }

        /* ROM adds a {low high} credits line after the name. */
        long pos = diku_lexer_tell(lex);
        char peek[256];
        if (diku_lexer_read_line(lex, peek, sizeof(peek))) {
            char *p = peek;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p == '{') {
                
                parse_rom_level_range(peek, &area->low_level, &area->high_level);
                if (!area->credits.str)
                    area->credits = diku_extract_tilded_name(arena, p);
                /* Next line should be the vnum range. */
                if (diku_lexer_read_number(lex, &area->low_vnum) == 0)
                    diku_lexer_read_number(lex, &area->high_vnum);
            } else if (isdigit((unsigned char)*p)) {
                /* Could be ROM/Merc vnum range, or old Diku continuation. */
                diku_lexer_seek(lex, pos);
                if (diku_lexer_read_number(lex, &area->low_vnum) == 0)
                    diku_lexer_read_number(lex, &area->high_vnum);
            } else {
                diku_lexer_seek(lex, pos);
            }
        }
    }

    int c;
    while ((c = diku_lexer_getc(lex)) != EOF) {
        if (c == '*') { diku_lexer_skip_line(lex); continue; }
        if (c == '#') {
            long hash_pos = diku_lexer_tell(lex) - 1;
            char peek[16];
            if (diku_lexer_read_word(lex, peek, 16)) {
                if (is_area_section_word(peek)) {
                    diku_lexer_seek(lex, hash_pos);
                    break;
                }
                if (strcmp(peek, "AREA") == 0 || strcmp(peek, "AREADATA") == 0) {
                    /* A late #AREA block (e.g. after a #Labeled line). */
                    diku_lexer_seek(lex, hash_pos);
                    char buf[256];
                    if (diku_lexer_read_line(lex, buf, sizeof(buf))) {
                        if (strstr(buf, "#AREADATA")) {
                            
                            parse_smaug_header(area, lex, arena);
                        } else if (strstr(buf, "#AREA")) {
                            char *brace = strchr(buf, '{');
                            if (brace) {
                                
                                parse_rom_level_range(buf, &area->low_level, &area->high_level);
                                char *close = strchr(brace, '}');
                                area->name = diku_extract_area_name_from_credits(arena, close ? close + 1 : brace + 1);
                            } else {
                                diku_lexer_read_string(lex, arena); /* filename */
                                area->name = diku_lexer_read_string(lex, arena);
                                long pos2 = diku_lexer_tell(lex);
                                char peek2[256];
                                if (diku_lexer_read_line(lex, peek2, sizeof(peek2))) {
                                    char *p2 = peek2;
                                    while (*p2 && isspace((unsigned char)*p2)) p2++;
                                    if (*p2 == '{') {
                                        
                                        parse_rom_level_range(peek2, &area->low_level, &area->high_level);
                                        if (!area->credits.str)
                                            area->credits = diku_extract_tilded_name(arena, p2);
                                        diku_lexer_read_number(lex, &area->low_vnum);
                                        diku_lexer_read_number(lex, &area->high_vnum);
                                    } else {
                                        diku_lexer_seek(lex, pos2);
                                    }
                                }
                            }
                        }
                    }
                    continue;
                }
                diku_lexer_skip_line(lex);
                continue;
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
                            room->flags = diku_decode_bitvector(token2);
                            diku_lexer_read_number(lex, &room->sector);
                        }
                    } else {
                        diku_lexer_seek(lex, after_t1);
                        if (isdigit(token1[0]) || token1[0] == '-') {
                            room->flags = diku_parse_numeric_flags(token1);
                        } else {
                            room->flags = diku_decode_bitvector(token1);
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

    /* ROM/SMAUG mobiles have a race keyword line (e.g. "human~") before the
       act flags. Consume it if present. */
    diku_lexer_skip_ws(lex);
    long race_pos = diku_lexer_tell(lex);
    const char *p = lex->pos;
    bool race_found = false;
    while (p < lex->end && !isspace((unsigned char)*p) && *p != '~') {
        if (!isalpha((unsigned char)*p)) break;
        p++;
    }
    if (p < lex->end && *p == '~') {
        /* The next token is a tilde-terminated alphabetic word: race. */
        diku_lexer_seek(lex, race_pos);
        diku_lexer_read_string(lex, arena); /* consume race */
        diku_lexer_skip_ws(lex);
    } else {
        diku_lexer_seek(lex, race_pos);
    }

    char buf[256];

    if (diku_lexer_read_word(lex, buf, 255)) {
        uint32_t flags = 0;
        if (isdigit(buf[0]) || buf[0] == '-') { flags = diku_parse_numeric_flags(buf); }
        else { flags = diku_decode_bitvector(buf); }
        mob->act_flags = flags;
    }
    if (diku_lexer_read_word(lex, buf, 255)) {
        uint32_t flags = 0;
        if (isdigit(buf[0]) || buf[0] == '-') { flags = diku_parse_numeric_flags(buf); }
        else { flags = diku_decode_bitvector(buf); }
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
            diku_string_t maybe = diku_lexer_read_string(lex, arena);
            if (maybe.len > 0) {
                /* ROM items place a single-word material (e.g. "iron~") between
                   the long description and the type line. If the next token is a
                   valid item type, treat this string as material. */
                bool single_word = true;
                for (size_t i = 0; i < maybe.len; i++) {
                    if (isspace((unsigned char)maybe.str[i])) { single_word = false; break; }
                }
                diku_lexer_skip_ws(lex);
                char type_test[64];
                long after_str = diku_lexer_tell(lex);
                bool next_is_type = false;
                if (diku_lexer_read_word(lex, type_test, 64)) {
                    if (isdigit(type_test[0]) || type_test[0] == '-') {
                        next_is_type = true;
                    } else if (diku_item_type_from_name(type_test) != 0) {
                        next_is_type = true;
                    }
                    diku_lexer_seek(lex, after_str);
                }
                if (single_word && next_is_type) {
                    item->material = maybe;
                } else {
                    item->description = maybe;
                }
            }
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
    char type_buf[256];
    if (diku_lexer_read_word(lex, type_buf, 255)) {
        if (isdigit(type_buf[0]) || type_buf[0] == '-') {
            item->type = (int)strtol(type_buf, NULL, 0);
        } else {
            item->type = diku_item_type_from_name(type_buf);
        }
    }

    char buf[256];
    if (diku_lexer_read_word(lex, buf, 255)) {
        uint32_t flags = 0;
        if (isdigit(buf[0]) || buf[0] == '-') { flags = diku_parse_numeric_flags(buf); }
        else { flags = diku_decode_bitvector(buf); }
        item->extra_flags = flags;
    }
    if (diku_lexer_read_word(lex, buf, 255)) {
        uint32_t flags = 0;
        if (isdigit(buf[0]) || buf[0] == '-') { flags = diku_parse_numeric_flags(buf); }
        else { flags = diku_decode_bitvector(buf); }
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
