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
/* Configuration constants - tweak for your fork                      */
/* ------------------------------------------------------------------ */
#define DIKU_MAX_EXITS      12    /* N NE E SE S SW W NW U D IN OUT */
#define DIKU_ARENA_BLOCK    (1024*1024*4)  /* 4 MiB slabs */
#define DIKU_VNUM_HASH_BITS 12    /* 4096 slots */
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
/* Arena allocator - bump pointer style, fast and cache-friendly       */
/* ------------------------------------------------------------------ */
struct arena_t {
    char *buf;
    size_t used;
    size_t cap;
    arena_t *next;
};

/* ------------------------------------------------------------------ */
/* String type - arena-backed, never free individually                */
/* ------------------------------------------------------------------ */
struct diku_string_t {
    char *str;
    size_t len;
};

/* ------------------------------------------------------------------ */
/* 3D Coordinate system                                               */
/* ------------------------------------------------------------------ */
struct coord3d_t {
    int x, y, z;
};

/* ------------------------------------------------------------------ */
/* Exit structure - supports linked list for forks with >6 dirs       */
/* ------------------------------------------------------------------ */
struct exit_t {
    int direction;           /* 0=N, 1=E, 2=S, 3=W, 4=U, 5=D */
    diku_string_t desc;      /* door/exit description */
    diku_string_t keywords;  /* door keywords */
    int flags;               /* door state bits (locked, closed, etc) */
    int key_vnum;            /* key object vnum, -1 if none */
    int to_vnum;             /* target room vnum before resolution */
    room_t *to_room;         /* pointer after resolve_graph() */
    exit_t *next;            /* for extra dirs if >6 (forks) */
};

/* ------------------------------------------------------------------ */
/* Room structure                                                     */
/* ------------------------------------------------------------------ */
struct room_t {
    int vnum;                    /* virtual number */
    diku_string_t name;          /* room name */
    diku_string_t desc;          /* room description */
    int flags;                   /* room flags */
    int sector;                  /* sector type (city, forest, etc) */
    exit_t *exits[DIKU_MAX_EXITS]; /* exits by direction */
    
    /* Extra descriptions (E lines) */
    struct {
        diku_string_t keywords;
        diku_string_t desc;
    } *extra_descs;
    int extra_desc_count;
    
    /* 3D coordinate (assigned by assign_coordinates) */
    coord3d_t coord;
    bool coord_assigned;
    
    /* Graph traversal markers */
    int traversal_mark;
    
    /* Room contents from resets */
    mobile_t **room_mobiles;
    int room_mobile_count;
    item_t **room_items;
    int room_item_count;
    
    /* User hook for game-specific data */
    void *user;
};

/* ------------------------------------------------------------------ */
/* Mobile/NPC structure - Merc/ROM compatible with extensions         */
/* ------------------------------------------------------------------ */
struct mobile_t {
    int vnum;
    diku_string_t name;         /* keywords for targeting */
    diku_string_t short_desc;   /* shown in room */
    diku_string_t long_desc;    /* shown on look */
    diku_string_t description;  /* detailed description (optional) */
    
    /* Core stats */
    int level;
    int alignment;
    int sex;                    /* 0=neutral, 1=male, 2=female */
    int race;                   /* some forks add this */
    
    /* Flags */
    uint32_t act_flags;
    uint32_t aff_flags;
    uint32_t off_flags;         /* offensive flags (ROM) */
    uint32_t imm_flags;         /* immunities */
    uint32_t res_flags;         /* resistances */
    uint32_t vuln_flags;        /* vulnerabilities */
    
    /* Combat stats */
    int hitroll;
    int damroll;
    int ac[4];                  /* armor class (pierce, bash, slash, exotic) */
    int hit[3];                 /* hit dice (number, type, bonus) */
    int mana[3];                /* mana dice */
    int damage[3];              /* damage dice */
    
    /* Position */
    int start_pos;
    int default_pos;
    
    /* Wealth and form */
    long gold;
    long silver;
    int form;                   /* body form */
    int parts;                  /* body parts */
    int size;                   /* tiny, small, medium, large, huge, giant */
    
    /* Material (ROM) */
    diku_string_t material;
    
    /* Fork-specific extra lines stored raw */
    char **extra_lines;
    int extra_count;
    
    /* Inventory from resets (G/E lines) */
    struct {
        item_t *item;
        int wear_loc;  /* -1 = held/given, >=0 = equipped */
    } *inventory;
    int inventory_count;
    
    /* User hook */
    void *user;
};

/* ------------------------------------------------------------------ */
/* Item/Object structure                                               */
/* ------------------------------------------------------------------ */
struct item_t {
    int vnum;
    diku_string_t name;         /* keywords */
    diku_string_t short_desc;   /* shown in inventory/equip */
    diku_string_t long_desc;    /* shown on ground */
    diku_string_t description;  /* detailed description (optional) */
    
    int type;                   /* item type (weapon, armor, etc) */
    int value[5];               /* type-specific values (some forks use 5) */
    int weight;
    int cost;                   /* base cost */
    int level;                  /* min level (ROM) */
    
    uint32_t extra_flags;
    uint32_t wear_flags;
    
    /* Material (ROM) */
    diku_string_t material;
    
    /* Affects (A lines) */
    struct {
        int location;
        int modifier;
    } *affects;
    int affect_count;
    
    /* Extra descriptions (E lines) */
    struct {
        diku_string_t keywords;
        diku_string_t desc;
    } *extra_descs;
    int extra_desc_count;
    
    /* Fork-specific extra lines stored raw */
    char **extra_lines;
    int extra_count;
    
    /* User hook */
    void *user;
};

/* ------------------------------------------------------------------ */
/* Vnum hash entry for fast lookup across areas                       */
/* ------------------------------------------------------------------ */
struct vnum_hash_entry_t {
    int vnum;
    room_t *room;
    vnum_hash_entry_t *next;
};

/* ------------------------------------------------------------------ */
/* Area structure - contains everything in one arena                   */
/* ------------------------------------------------------------------ */
struct area_t {
    /* Area header info */
    diku_string_t name;
    diku_string_t filename;     /* loaded from */
    diku_string_t builders;     /* K line */
    diku_string_t credits;      /* some forks have this */
    
    int low_level, high_level;  /* I line */
    int low_vnum, high_vnum;    /* V line */
    int security;               /* N line - security level */
    int version;                /* X line - area version */
    int reset_interval;         /* F line - reset frequency */
    
    /* Optional header fields */
    diku_string_t ambient_sound; /* U line */
    diku_string_t owner;         /* O line */
    diku_string_t reset_msg;     /* R line */
    diku_string_t weather;       /* W line */
    diku_string_t pay_info;      /* P line */
    diku_string_t teleport_info; /* T line */
    diku_string_t magic_info;    /* M line */
    
    /* Rooms - stored in array for iteration, hash for lookup */
    room_t *rooms;
    int room_count;
    room_t **rooms_by_vnum;     /* hash table for O(1) lookup */
    
    /* Mobiles */
    mobile_t *mobiles;
    int mobile_count;
    
    /* Items */
    item_t *items;
    int item_count;
    
    /* Raw sections stored for forks - parsed on demand */
    char **helps_raw;           /* #HELPS section */
    int helps_line_count;
    
    char **resets_raw;          /* #RESETS section */
    int resets_line_count;
    
    char **shops_raw;           /* #SHOPS section */
    int shops_line_count;
    
    char **specials_raw;        /* #SPECIALS section */
    int specials_line_count;
    
    char **objfuns_raw;         /* #OBJFUNS section */
    int objfuns_line_count;
    
    /* Extra sections for unknown/fork-specific sections */
    struct {
        diku_string_t section_name;
        char **lines;
        int line_count;
    } *extra_sections;
    int extra_section_count;
    
    /* Arena - all memory lives here */
    arena_t *arena;
    
    /* Next area in linked list */
    area_t *next;
};

/* ------------------------------------------------------------------ */
/* Global state for coordinate assignment                             */
/* ------------------------------------------------------------------ */
typedef struct {
    int traversal_counter;
    int max_x, max_y, max_z;
    int min_x, min_y, min_z;
} diku_global_state_t;

extern diku_global_state_t diku_global;

/* ------------------------------------------------------------------ */
/* Progress callbacks                                                 */
/* ------------------------------------------------------------------ */
typedef void (*diku_progress_cb_t)(const char *operation, int current, int total, const char *detail, void *user);
void diku_set_progress_callback(diku_progress_cb_t cb, void *user);

/* ------------------------------------------------------------------ */
/* Arena allocator API                                                */
/* ------------------------------------------------------------------ */
arena_t *arena_create(void);
void *arena_alloc(arena_t *a, size_t n);
void *arena_alloc_aligned(arena_t *a, size_t n, size_t align);
char *arena_strdup(arena_t *a, const char *s);
diku_string_t arena_strndup(arena_t *a, const char *s, size_t len);
diku_string_t arena_strdup_diku(arena_t *a, const char *s); /* handles ~ terminator */
void arena_free_all(arena_t *a);

/* ------------------------------------------------------------------ */
/* Parser core - low level file reading                               */
/* ------------------------------------------------------------------ */
int diku_fread_number(FILE *fp, int *out);
diku_string_t diku_fread_string(FILE *fp, arena_t *arena);
diku_string_t diku_fread_string_eol(FILE *fp, arena_t *arena);
char *diku_fread_word(FILE *fp, arena_t *arena);
char *diku_fread_line(FILE *fp, arena_t *arena);
void diku_fread_to_endline(FILE *fp);
bool diku_fread_letter(FILE *fp, char expected);

/* ------------------------------------------------------------------ */
/* Section parsers                                                    */
/* ------------------------------------------------------------------ */
area_t *diku_parse_area_header(FILE *fp, arena_t *arena);
room_t *diku_parse_room(FILE *fp, arena_t *arena, int *vnum_out);
mobile_t *diku_parse_mobile(FILE *fp, arena_t *arena, int *vnum_out);
item_t *diku_parse_item(FILE *fp, arena_t *arena, int *vnum_out);

/* ------------------------------------------------------------------ */
/* Main entry points                                                  */
/* ------------------------------------------------------------------ */

/* Parse a single area file. Returns NULL on failure (check errno). */
area_t *diku_parse_file(const char *filename);

/* Parse from an open FILE*. Returns NULL on failure. */
area_t *diku_parse_fp(FILE *fp, const char *filename);

/* Parse individual classic DikuMUD/CircleMUD files into an area */
bool diku_parse_wld_fp(FILE *fp, area_t *area);
bool diku_parse_mob_fp(FILE *fp, area_t *area);
bool diku_parse_obj_fp(FILE *fp, area_t *area);
bool diku_parse_zon_fp(FILE *fp, area_t *area);

/* Load a 4-file package by basename (e.g. "areas/1") */
area_t *diku_parse_package(const char *base_path);
area_t *diku_parse_package_files(const char *wld, const char *mob, const char *obj, const char *zon);

/* Load all .are files or all multi-file packages from a folder */
area_t *diku_load_folder_are(const char *folder_path);
area_t *diku_load_folder_packages(const char *folder_path);

/* Parse resets and populate room contents (call after diku_parse_fp) */
void diku_parse_resets(area_t *area);

/* After loading multiple areas, call this to wire the graph. */
void diku_resolve_graph(area_t **areas, int area_count);

/* Alternative: resolve using a global vnum hash */
void diku_resolve_graph_global(area_t *areas);

/* ------------------------------------------------------------------ */
/* 3D Coordinate assignment                                           */
/* ------------------------------------------------------------------ */

/* Find the "central" room in an area (best-effort heuristic) */
room_t *diku_find_central_room(area_t *area);

/* Assign 3D coordinates starting from a root room at (0,0,0) */
void diku_assign_coordinates(area_t *area, room_t *root);

/* Assign coordinates for all areas (finds central room automatically) */
void diku_assign_coordinates_all(area_t *areas);

/* Assign coordinates across all loaded areas with a single BFS (cross-area consistent) */
void diku_assign_coordinates_multi(area_t *areas);

/* Get room at specific coordinates, or NULL if none */
room_t *diku_room_at_coord(area_t *area, int x, int y, int z);

/* Check if coordinates are occupied */
bool diku_coord_occupied(area_t *area, int x, int y, int z, room_t *exclude);

/* Shift all coordinates to center around origin */
void diku_center_coordinates(area_t *area);

/* Get bounds of assigned coordinates */
void diku_get_coord_bounds(area_t *area, int *min_x, int *max_x, 
                           int *min_y, int *max_y, int *min_z, int *max_z);

/* ------------------------------------------------------------------ */
/* Lookup functions                                                   */
/* ------------------------------------------------------------------ */
room_t *diku_find_room(area_t *area, int vnum);
room_t *diku_find_room_global(area_t *areas, int vnum);
mobile_t *diku_find_mobile(area_t *area, int vnum);
item_t *diku_find_item(area_t *area, int vnum);

/* Vnum hash for global lookups */
vnum_hash_entry_t **diku_build_vnum_hash(area_t *areas);
void diku_free_vnum_hash(vnum_hash_entry_t **hash);
room_t *diku_vnum_hash_lookup(vnum_hash_entry_t **hash, int vnum);

/* ------------------------------------------------------------------ */
/* Utility functions                                                  */
/* ------------------------------------------------------------------ */
void diku_free_area(area_t *area);
void diku_free_all_areas(area_t *areas);

/* Print area info to stdout (for debugging) */
void diku_print_area_info(const area_t *area);
void diku_print_room(const room_t *room);
void diku_print_mobile(const mobile_t *mob);
void diku_print_item(const item_t *item);
void diku_print_graph(const area_t *area);
void diku_print_coordinates(area_t *area);

/* Get direction name */
const char *diku_dir_name(int dir);
const char *diku_dir_name_short(int dir);

/* Get reverse direction */
int diku_reverse_dir(int dir);

/* Check if room has exit in direction */
bool diku_has_exit(const room_t *room, int dir);

/* Get exit destination vnum (before resolution) */
int diku_get_exit_vnum(const room_t *room, int dir);

/* Count total exits in a room */
int diku_count_exits(const room_t *room);

/* Calculate graph diameter (longest shortest path) */
int diku_graph_diameter(area_t *area);

/* Exit symmetry check */
int diku_check_exit_symmetry(const area_t *area, int *out_total, int *out_asymmetric);
void diku_print_exit_symmetry(const area_t *area);

/* ------------------------------------------------------------------ */
/* Fork compatibility helpers                                         */
/* ------------------------------------------------------------------ */

/* Detect area format variant */
typedef enum {
    DIKU_FMT_UNKNOWN = 0,
    DIKU_FMT_DIKU,      /* Original Diku */
    DIKU_FMT_MERC,      /* Merc 2.x */
    DIKU_FMT_ROM,       /* ROM 2.4+ */
    DIKU_FMT_CIRCLE,    /* CircleMUD */
    DIKU_FMT_SMAUG,     /* SMAUG */
    DIKU_FMT_CUSTOM     /* Unknown/custom */
} diku_format_t;

diku_format_t diku_detect_format(const area_t *area);
const char *diku_format_name(diku_format_t fmt);

/* Sector type names */
const char *diku_sector_name(int sector);

/* Room flag names (Merc/ROM compatible) */
const char *diku_room_flag_name(uint32_t flag);

/* Item type names */
const char *diku_item_type_name(int type);

#ifdef __cplusplus
}
#endif

#endif /* DIKU_PARSER_H */
