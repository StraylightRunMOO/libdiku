#ifndef DIKU_TYPES_H
#define DIKU_TYPES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <math.h>

#include <memento.h>
#include "diku/config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */
typedef struct diku_string_t diku_string_t;
typedef struct exit_t exit_t;
typedef struct room_t room_t;
typedef struct mobile_t mobile_t;
typedef struct item_t item_t;
typedef struct area_t area_t;
typedef struct coord3d_t coord3d_t;
typedef struct vnum_hash_entry_t vnum_hash_entry_t;
typedef struct diku_lexer_t diku_lexer_t;

/* ------------------------------------------------------------------ */
/* String & coordinate types                                          */
/* ------------------------------------------------------------------ */
struct diku_string_t {
    char *str;
    size_t len;
};

struct coord3d_t {
    int x, y, z;
};

/* ------------------------------------------------------------------ */
/* Core structs                                                       */
/* ------------------------------------------------------------------ */
struct exit_t {
    int direction;
    diku_string_t desc;
    diku_string_t keywords;
    int flags;
    int key_vnum;
    int to_vnum;
    room_t *to_room;
    exit_t *next;
};

typedef struct room_t room_t;
struct room_t {
    int vnum;
    diku_string_t name;
    diku_string_t desc;
    int flags;
    int sector;
    exit_t *exits[DIKU_MAX_EXITS];
    struct { diku_string_t keywords; diku_string_t desc; } *extra_descs;
    int extra_desc_count;
    coord3d_t coord;
    bool coord_assigned;
    uint32_t traversal_epoch;
    int traversal_dist;
    mobile_t **room_mobiles;
    int room_mobile_count;
    item_t **room_items;
    int room_item_count;
    void *user;
} __attribute__((aligned(64)));

typedef struct mobile_t mobile_t;
struct mobile_t {
    int vnum;
    diku_string_t name, short_desc, long_desc, description;
    int level, alignment, sex, race;
    uint32_t act_flags, aff_flags, off_flags, imm_flags, res_flags, vuln_flags;
    int hitroll, damroll;
    int ac[4], hit[3], mana[3], damage[3];
    int start_pos, default_pos;
    long gold, silver;
    int form, parts, size;
    diku_string_t material;
    char **extra_lines; int extra_count;
    struct { item_t *item; int wear_loc; } *inventory;
    int inventory_count;
    void *user;
} __attribute__((aligned(64)));

typedef struct item_t item_t;
struct item_t {
    int vnum;
    diku_string_t name, short_desc, long_desc, description;
    int type, value[5], weight, cost, level;
    uint32_t extra_flags, wear_flags;
    diku_string_t material;
    struct { int location; int modifier; } *affects;
    int affect_count;
    struct { diku_string_t keywords; diku_string_t desc; } *extra_descs;
    int extra_desc_count;
    char **extra_lines; int extra_count;
    void *user;
} __attribute__((aligned(64)));

/* ------------------------------------------------------------------ */
/* Fork detection enum                                                */
/* ------------------------------------------------------------------ */
typedef enum {
    DIKU_FMT_UNKNOWN = 0,
    DIKU_FMT_DIKU,
    DIKU_FMT_MERC,
    DIKU_FMT_ROM,
    DIKU_FMT_CIRCLE,
    DIKU_FMT_SMAUG,
    DIKU_FMT_CUSTOM
} diku_format_t;

typedef struct area_t area_t;
struct area_t {
    diku_string_t name, filename, builders, credits;
    int low_level, high_level, low_vnum, high_vnum;
    int security, version, reset_interval;
    diku_string_t ambient_sound, owner, reset_msg, weather, pay_info, teleport_info, magic_info;
    diku_format_t format;
    room_t *rooms; int room_count;
    room_t **rooms_by_vnum;
    mobile_t *mobiles; int mobile_count;
    item_t *items; int item_count;
    char **helps_raw, **resets_raw, **shops_raw, **specials_raw, **objfuns_raw;
    int helps_line_count, resets_line_count, shops_line_count, specials_line_count, objfuns_line_count;
    struct { diku_string_t section_name; char **lines; int line_count; } *extra_sections;
    int extra_section_count;
    memento_arena_t *arena;
    area_t *next;
} __attribute__((aligned(64)));

/* ------------------------------------------------------------------ */
/* Progress callback type                                             */
/* ------------------------------------------------------------------ */
typedef void (*diku_progress_cb_t)(const char *operation, int current, int total, const char *detail, void *user);

/* ------------------------------------------------------------------ */
/* Lexer types                                                        */
/* ------------------------------------------------------------------ */
typedef enum {
    TOK_EOF,
    TOK_HASH_SECTION,
    TOK_NUMBER,
    TOK_STRING,
    TOK_WORD,
    TOK_EOL,
    TOK_UNKNOWN
} diku_token_type_t;

typedef struct {
    const char *start;
    size_t      len;
    diku_token_type_t type;
    int         line;
    int         col;
} diku_token_t;

struct diku_lexer_t {
    memento_arena_t *arena;
    const char *buf;
    const char *pos;
    const char *end;
    int         line;
    int         col;
    bool        owns_buf;
};

#ifdef __cplusplus
}
#endif
#endif /* DIKU_TYPES_H */
