#ifndef DIKU_PARSER_H
#define DIKU_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Configuration constants                                            */
/* ------------------------------------------------------------------ */
#define DIKU_MAX_EXITS      12
#define DIKU_ARENA_BLOCK    (1024*1024*4)
#define DIKU_VNUM_HASH_BITS 12
#define DIKU_VNUM_HASH_SIZE (1 << DIKU_VNUM_HASH_BITS)
#define DIKU_VNUM_HASH_MASK (DIKU_VNUM_HASH_SIZE - 1)

/* Direction constants */
#define DIR_NORTH       0
#define DIR_EAST        1
#define DIR_SOUTH       2
#define DIR_WEST        3
#define DIR_UP          4
#define DIR_DOWN        5
#define DIR_NORTHEAST   6
#define DIR_NORTHWEST   7
#define DIR_SOUTHEAST   8
#define DIR_SOUTHWEST   9
#define DIR_IN          10
#define DIR_OUT         11

/* Direction vector offsets for 3D coordinates */
static const int dir_offset[12][3] = {
    { 0,  1,  0},  /* North:      +Y */
    { 1,  0,  0},  /* East:       +X */
    { 0, -1,  0},  /* South:      -Y */
    {-1,  0,  0},  /* West:       -X */
    { 0,  0,  1},  /* Up:         +Z */
    { 0,  0, -1},  /* Down:       -Z */
    { 1,  1,  0},  /* Northeast:  +X +Y */
    {-1,  1,  0},  /* Northwest:  -X +Y */
    { 1, -1,  0},  /* Southeast:  +X -Y */
    {-1, -1,  0},  /* Southwest:  -X -Y */
    { 0,  0,  0},  /* In:          0  0 */
    { 0,  0,  0},  /* Out:         0  0 */
};

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */
typedef struct arena_t arena_t;
typedef struct diku_string_t diku_string_t;
typedef struct exit_t exit_t;
typedef struct room_t room_t;
typedef struct mobile_t mobile_t;
typedef struct item_t item_t;
typedef struct area_t area_t;
typedef struct coord3d_t coord3d_t;
typedef struct vnum_hash_entry_t vnum_hash_entry_t;

/* ------------------------------------------------------------------ */
/* Arena & string types                                               */
/* ------------------------------------------------------------------ */
struct arena_t {
    char *buf;
    size_t used;
    size_t cap;
    arena_t *next;
};

struct diku_string_t {
    char *str;
    size_t len;
};

struct coord3d_t {
    int x, y, z;
};

/* ------------------------------------------------------------------ */
/* Core structs — C99 + __attribute__((aligned(64))) for hot paths   */
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

/* Forward typedef + aligned struct definition (C99 legal) */
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
    int traversal_mark;
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

typedef struct area_t area_t;
struct area_t {
    diku_string_t name, filename, builders, credits;
    int low_level, high_level, low_vnum, high_vnum;
    int security, version, reset_interval;
    diku_string_t ambient_sound, owner, reset_msg, weather, pay_info, teleport_info, magic_info;
    room_t *rooms; int room_count;
    room_t **rooms_by_vnum;
    mobile_t *mobiles; int mobile_count;
    item_t *items; int item_count;
    char **helps_raw, **resets_raw, **shops_raw, **specials_raw, **objfuns_raw;
    int helps_line_count, resets_line_count, shops_line_count, specials_line_count, objfuns_line_count;
    struct { diku_string_t section_name; char **lines; int line_count; } *extra_sections;
    int extra_section_count;
    arena_t *arena;
    area_t *next;
} __attribute__((aligned(64)));

/* ------------------------------------------------------------------ */
/* Global & progress                                                  */
/* ------------------------------------------------------------------ */
typedef struct {
    int traversal_counter;
    int min_x, max_x, min_y, max_y, min_z, max_z;
} diku_global_state_t;
extern diku_global_state_t diku_global;

typedef void (*diku_progress_cb_t)(const char *operation, int current, int total, const char *detail, void *user);
void diku_set_progress_callback(diku_progress_cb_t cb, void *user);

/* ------------------------------------------------------------------ */
/* Arena & low-level parser API                                       */
/* ------------------------------------------------------------------ */
arena_t *arena_create(void);
void *arena_alloc(arena_t *a, size_t n);
void *arena_alloc_aligned(arena_t *a, size_t n, size_t align);
char *arena_strdup(arena_t *a, const char *s);
diku_string_t arena_strndup(arena_t *a, const char *s, size_t len);
diku_string_t arena_strdup_diku(arena_t *a, const char *s);
void arena_free_all(arena_t *a);

int diku_fread_number(FILE *fp, int *out);
diku_string_t diku_fread_string(FILE *fp, arena_t *arena);
diku_string_t diku_fread_string_eol(FILE *fp, arena_t *arena);
char *diku_fread_word(FILE *fp, arena_t *arena);
char *diku_fread_line(FILE *fp, arena_t *arena);
void diku_fread_to_endline(FILE *fp);
bool diku_fread_letter(FILE *fp, char expected);

/* ------------------------------------------------------------------ */
/* Section parsers & main entry points                                */
/* ------------------------------------------------------------------ */
area_t *diku_parse_area_header(FILE *fp, arena_t *arena);
room_t *diku_parse_room(FILE *fp, arena_t *arena, int *vnum_out);
mobile_t *diku_parse_mobile(FILE *fp, arena_t *arena, int *vnum_out);
item_t *diku_parse_item(FILE *fp, arena_t *arena, int *vnum_out);

area_t *diku_parse_file(const char *filename);
area_t *diku_parse_fp(FILE *fp, const char *filename);
bool diku_parse_wld_fp(FILE *fp, area_t *area);
bool diku_parse_mob_fp(FILE *fp, area_t *area);
bool diku_parse_obj_fp(FILE *fp, area_t *area);
bool diku_parse_zon_fp(FILE *fp, area_t *area);
area_t *diku_parse_package(const char *base_path);
area_t *diku_parse_package_files(const char *wld, const char *mob, const char *obj, const char *zon);
area_t *diku_load_folder_are(const char *folder_path);
area_t *diku_load_folder_packages(const char *folder_path);
void diku_parse_resets(area_t *area);

/* ------------------------------------------------------------------ */
/* Graph resolution & lookups (explicitly declared for your .c)       */
/* ------------------------------------------------------------------ */
void diku_resolve_graph(area_t **areas, int area_count);
void diku_resolve_graph_global(area_t *areas);
room_t *diku_find_room(area_t *area, int vnum);
room_t *diku_find_room_global(area_t *areas, int vnum);
mobile_t *diku_find_mobile(area_t *area, int vnum);
item_t *diku_find_item(area_t *area, int vnum);

/* ------------------------------------------------------------------ */
/* Coordinate assignment                                              */
/* ------------------------------------------------------------------ */
room_t *diku_find_central_room(area_t *area);
void diku_assign_coordinates(area_t *area, room_t *root);
void diku_assign_coordinates_all(area_t *areas);
void diku_assign_coordinates_multi(area_t *areas);
room_t *diku_room_at_coord(area_t *area, int x, int y, int z);
bool diku_coord_occupied(area_t *area, int x, int y, int z, room_t *exclude);
void diku_center_coordinates(area_t *area);
void diku_get_coord_bounds(area_t *area, int *min_x, int *max_x, int *min_y, int *max_y, int *min_z, int *max_z);

/* ------------------------------------------------------------------ */
/* Utility & debug                                                    */
/* ------------------------------------------------------------------ */
void diku_free_area(area_t *area);
void diku_free_all_areas(area_t *areas);
void diku_print_area_info(const area_t *area);
void diku_print_room(const room_t *room);
void diku_print_mobile(const mobile_t *mob);
void diku_print_item(const item_t *item);
void diku_print_graph(const area_t *area);
void diku_print_coordinates(area_t *area);
const char *diku_dir_name(int dir);
const char *diku_dir_name_short(int dir);
int diku_reverse_dir(int dir);
bool diku_has_exit(const room_t *room, int dir);
int diku_get_exit_vnum(const room_t *room, int dir);
int diku_count_exits(const room_t *room);
int diku_graph_diameter(area_t *area);
int diku_check_exit_symmetry(const area_t *area, int *out_total, int *out_asymmetric);
void diku_print_exit_symmetry(const area_t *area);

/* ------------------------------------------------------------------ */
/* Fork detection                                                     */
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

diku_format_t diku_detect_format(const area_t *area);
const char *diku_format_name(diku_format_t fmt);
const char *diku_sector_name(int sector);
const char *diku_item_type_name(int type);

#ifdef __cplusplus
}
#endif
#endif

