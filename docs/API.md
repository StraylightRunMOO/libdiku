# libdiku API Reference

This document describes every public symbol in the libdiku C API and the C++17 wrapper. All C symbols are declared in `include/diku.h` (the umbrella header) and are wrapped in `extern "C"` when compiled as C++.

---

## Table of Contents

- [Configuration (`diku/config.h`)](#configuration)
- [Types (`diku/types.h`)](#types)
- [Arena (`diku/arena.h`)](#arena)
- [Context (`diku/context.h`)](#context)
- [Lexer (`diku/lexer.h`)](#lexer)
- [Utilities (`diku/util.h`)](#utilities)
- [Find (`diku/find.h`)](#find)
- [Sections (`diku/sections.h`)](#sections)
- [Resets (`diku/resets.h`)](#resets)
- [Format (`diku/format.h`)](#format)
- [Graph (`diku/graph.h`)](#graph)
- [Coordinates (`diku/coords.h`)](#coordinates)
- [Statistics (`diku/stats.h`)](#statistics)
- [Debug (`diku/debug.h`)](#debug)
- [Pager (`diku/pager.h`)](#pager)
- [C++ Wrapper (`diku.hpp`)](#c-wrapper)

---

## Configuration

**Header:** `diku/config.h`

### Constants

| Name | Value | Description |
|------|-------|-------------|
| `DIKU_MAX_EXITS` | `12` | Maximum exit directions per room |
| `DIKU_VNUM_HASH_BITS` | `12` | Bit width of the vnum hash table |
| `DIKU_VNUM_HASH_SIZE` | `4096` | Number of buckets in the vnum hash |
| `DIKU_VNUM_HASH_MASK` | `4095` | Mask for vnum hash computation |

### Direction Constants

```c
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
```

### `dir_offset`

```c
static const int dir_offset[12][3];
```

Coordinate delta for each direction. Indexed by `DIR_*` constant. Returns `{dx, dy, dz}`.

- North: `{0, +1, 0}`
- East: `{+1, 0, 0}`
- South: `{0, -1, 0}`
- West: `{-1, 0, 0}`
- Up: `{0, 0, +1}`
- Down: `{0, 0, -1}`

---

## Types

**Header:** `diku/types.h`

### `diku_string_t`

```c
typedef struct {
    char *str;
    size_t len;
} diku_string_t;
```

Length-delimited string. All strings are null-terminated for convenience, but `len` is authoritative. Memory lives in the area's arena.

### `coord3d_t`

```c
typedef struct {
    int x, y, z;
} coord3d_t;
```

3D integer coordinate.

### `exit_t`

```c
typedef struct exit_t {
    int direction;
    diku_string_t desc;
    diku_string_t keywords;
    int flags;
    int key_vnum;
    int to_vnum;
    room_t *to_room;    /* valid after graph resolution */
    exit_t *next;
} exit_t;
```

Room exit. `to_vnum` is the raw target from the file; `to_room` is resolved by `diku_resolve_graph()` or `diku_resolve_graph_global()`. The `next` pointer chains extra exits beyond the 12 cardinal directions.

### `room_t`

```c
typedef struct room_t {
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

    uint32_t traversal_epoch;   /* for BFS epoch tracking */
    int traversal_dist;

    mobile_t **room_mobiles;    /* populated by reset parsing */
    int room_mobile_count;
    item_t **room_items;        /* populated by reset parsing */
    int room_item_count;

    void *user;                 /* application hook */
} room_t;
```

A single room. `traversal_epoch` and `traversal_dist` are used internally by BFS algorithms; do not modify.

### `mobile_t`

```c
typedef struct mobile_t {
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
    char **extra_lines;
    int extra_count;
    struct { item_t *item; int wear_loc; } *inventory;
    int inventory_count;
    void *user;
} mobile_t;
```

A mobile (NPC). The `inventory` array is populated by reset parsing (`G` and `E` commands).

### `item_t`

```c
typedef struct item_t {
    int vnum;
    diku_string_t name, short_desc, long_desc, description;
    int type, value[5], weight, cost, level;
    uint32_t extra_flags, wear_flags;
    diku_string_t material;
    struct { int location; int modifier; } *affects;
    int affect_count;
    struct { diku_string_t keywords; diku_string_t desc; } *extra_descs;
    int extra_desc_count;
    char **extra_lines;
    int extra_count;
    void *user;
} item_t;
```

An object/item.

### `area_t`

```c
typedef struct area_t {
    diku_string_t name, filename, builders, credits;
    int low_level, high_level, low_vnum, high_vnum;
    int security, version, reset_interval;
    diku_string_t ambient_sound, owner, reset_msg, weather, pay_info, teleport_info, magic_info;
    diku_format_t format;       /* detected fork (DIKU_FMT_*) */

    room_t *rooms; int room_count;
    room_t **rooms_by_vnum;     /* vnum hash for O(1) lookup */

    mobile_t *mobiles; int mobile_count;
    item_t *items; int item_count;

    char **helps_raw; int helps_line_count;
    char **resets_raw; int resets_line_count;
    char **shops_raw; int shops_line_count;
    char **specials_raw; int specials_line_count;
    char **objfuns_raw; int objfuns_line_count;

    struct { diku_string_t section_name; char **lines; int line_count; } *extra_sections;
    int extra_section_count;

    memento_arena_t *arena;     /* owns all memory for this area */
    area_t *next;               /* linked list for multi-area loads */
} area_t;
```

An entire parsed area. All memory is allocated from `arena`. Freeing the area frees everything at once. The `next` pointer links multiple areas loaded from a folder.

### `diku_progress_cb_t`

```c
typedef void (*diku_progress_cb_t)(const char *operation, int current, int total,
                                    const char *detail, void *user);
```

Progress callback signature. Set on a context via `diku_context_set_progress()`.

### `diku_lexer_t`

```c
typedef struct diku_lexer_t {
    memento_arena_t *arena;
    const char *buf;
    const char *pos;
    const char *end;
    int line;
    int col;
    bool owns_buf;
} diku_lexer_t;
```

Memory-backed lexer. Initialize with `diku_lexer_init_file()`, `diku_lexer_init_fp()`, or `diku_lexer_init_buf()`.

### `diku_token_type_t`

```c
typedef enum {
    TOK_EOF,
    TOK_HASH_SECTION,
    TOK_NUMBER,
    TOK_STRING,
    TOK_WORD,
    TOK_EOL,
    TOK_UNKNOWN
} diku_token_type_t;
```

### `diku_token_t`

```c
typedef struct {
    const char *start;
    size_t len;
    diku_token_type_t type;
    int line;
    int col;
} diku_token_t;
```

### `diku_format_t`

```c
typedef enum {
    DIKU_FMT_UNKNOWN = 0,
    DIKU_FMT_DIKU,
    DIKU_FMT_MERC,
    DIKU_FMT_ROM,
    DIKU_FMT_CIRCLE,
    DIKU_FMT_SMAUG,
    DIKU_FMT_CUSTOM
} diku_format_t;
```

Detected area file format.

---

## Arena

**Header:** `diku/arena.h`

All arena functions are `static inline` and are always available (not gated by `DIKU_PARSER_IMPLEMENTATION`).

### `diku_arena_create`

```c
memento_arena_t *diku_arena_create(void);
```

Create a new 4 MiB Memento arena. Initializes Memento on first call.

**Returns:** New arena, or `NULL` on failure.

### `diku_arena_alloc`

```c
void *diku_arena_alloc(memento_arena_t *a, size_t n);
```

Allocate `n` bytes from the arena with 8-byte alignment.

### `diku_arena_alloc_aligned`

```c
void *diku_arena_alloc_aligned(memento_arena_t *a, size_t n, size_t align);
```

Allocate `n` bytes with custom alignment.

### `diku_arena_strdup`

```c
char *diku_arena_strdup(memento_arena_t *a, const char *s);
```

Duplicate a C string into the arena.

### `diku_arena_strndup`

```c
diku_string_t diku_arena_strndup(memento_arena_t *a, const char *s, size_t len);
```

Duplicate `len` bytes into a `diku_string_t` on the arena.

### `diku_arena_strdup_diku`

```c
diku_string_t diku_arena_strdup_diku(memento_arena_t *a, const char *s);
```

Read a DikuMUD `~`-terminated string from `s`. Strips trailing whitespace/newlines before the `~`.

### `diku_arena_free_all`

```c
void diku_arena_free_all(memento_arena_t *a);
```

Destroy the arena and all memory allocated from it.

---

## Context

**Header:** `diku/context.h`

The context object replaces all mutable global state. Create one context per thread or per logical operation. The context does **not** own areas — it is scratch space for coordinate hashes, BFS epochs, and progress callbacks.

### `diku_context_create`

```c
diku_context_t *diku_context_create(void);
```

Create a new context with its own scratch arena.

**Returns:** New context, or `NULL` on failure.

### `diku_context_destroy`

```c
void diku_context_destroy(diku_context_t *ctx);
```

Destroy the context and its scratch arena. Does **not** free any parsed areas.

### `diku_context_set_progress`

```c
void diku_context_set_progress(diku_context_t *ctx,
                                diku_progress_cb_t cb, void *user);
```

Set a progress callback on the context. The callback is invoked during folder loading operations. Pass `NULL` for `cb` to clear.

### `diku_context_set_verbose`

```c
void diku_context_set_verbose(diku_context_t *ctx, bool verbose);
```

Set the verbose flag on the context. When enabled, the library may emit additional diagnostic output (for example, progress information during long operations). By default the context is non-verbose.

---

## Lexer

**Header:** `diku/lexer.h`

### `diku_lexer_init_file`

```c
bool diku_lexer_init_file(diku_lexer_t *lex, const char *filename);
```

Read an entire file into memory and initialize the lexer.

**Returns:** `true` on success. The lexer owns the buffer and will `free()` it on cleanup.

### `diku_lexer_init_fp`

```c
bool diku_lexer_init_fp(diku_lexer_t *lex, FILE *fp);
```

Read from an open `FILE*` into memory and initialize the lexer. The `FILE*` is **not** closed by the lexer.

### `diku_lexer_init_buf`

```c
void diku_lexer_init_buf(diku_lexer_t *lex, const char *buf, size_t len);
```

Initialize the lexer from an existing memory buffer. The lexer does **not** own the buffer.

### `diku_lexer_cleanup`

```c
void diku_lexer_cleanup(diku_lexer_t *lex);
```

Free the lexer's buffer if it owns it (`owns_buf == true`).

### `diku_lexer_next`

```c
diku_token_t diku_lexer_next(diku_lexer_t *lex);
```

Return the next token. Skips whitespace, `\r`, and `*` comments.

### `diku_lexer_getc`

```c
int diku_lexer_getc(diku_lexer_t *lex);
```

Return the next raw character and advance.

### `diku_lexer_ungetc`

```c
void diku_lexer_ungetc(diku_lexer_t *lex, int c);
```

Step back one character. The `c` argument is ignored; the function simply decrements `pos`.

### `diku_lexer_tell`

```c
long diku_lexer_tell(diku_lexer_t *lex);
```

Current byte offset from the start of the buffer.

### `diku_lexer_seek`

```c
void diku_lexer_seek(diku_lexer_t *lex, long pos);
```

Seek to an absolute byte offset. Recalculates `line` and `col`.

### `diku_lexer_peek`

```c
int diku_lexer_peek(diku_lexer_t *lex);
```

Return the next character without advancing. Returns `EOF` at end of buffer.

### `diku_lexer_skip_ws`

```c
void diku_lexer_skip_ws(diku_lexer_t *lex);
```

Skip whitespace and comments.

### `diku_lexer_skip_line`

```c
void diku_lexer_skip_line(diku_lexer_t *lex);
```

Skip to the start of the next line.

### `diku_lexer_read_word`

```c
bool diku_lexer_read_word(diku_lexer_t *lex, char *out, size_t max);
```

Read the next whitespace-delimited token into `out` (max `max` bytes).

**Returns:** `true` if a word was read.

### `diku_lexer_read_line`

```c
bool diku_lexer_read_line(diku_lexer_t *lex, char *out, size_t max);
```

Read until newline into `out`. Strips the trailing newline.

### `diku_lexer_read_number`

```c
int diku_lexer_read_number(diku_lexer_t *lex, int *out);
```

Read a signed decimal integer. Returns `0` on success, `-1` on failure.

### `diku_lexer_read_string`

```c
diku_string_t diku_lexer_read_string(diku_lexer_t *lex, memento_arena_t *arena);
```

Read a `~`-terminated string into the arena. Returns `{NULL, 0}` for an empty string (`~~`).

### `diku_lexer_read_word_dup`

```c
char *diku_lexer_read_word_dup(diku_lexer_t *lex, memento_arena_t *arena);
```

Read a word and duplicate it into the arena.

### `diku_lexer_eof`

```c
bool diku_lexer_eof(diku_lexer_t *lex);
```

Returns `true` if only whitespace/comments remain.

---

## Utilities

**Header:** `diku/util.h`

All utility functions are `static inline` pure helpers and are always available.

### `diku_dir_name`

```c
const char *diku_dir_name(int dir);
```

Return the full direction name (e.g., `"north"`, `"northeast"`). Returns `"unknown"` for out-of-range.

### `diku_dir_name_short`

```c
const char *diku_dir_name_short(int dir);
```

Return the abbreviated direction name (e.g., `"n"`, `"ne"`). Returns `"?"` for out-of-range.

### `diku_reverse_dir`

```c
int diku_reverse_dir(int dir);
```

Return the opposite direction. Returns `-1` for out-of-range.

### `diku_has_exit`

```c
bool diku_has_exit(const room_t *room, int dir);
```

Returns `true` if the room has an exit in direction `dir` with a positive `to_vnum`.

### `diku_get_exit_vnum`

```c
int diku_get_exit_vnum(const room_t *room, int dir);
```

Return the `to_vnum` of the exit in direction `dir`, or `-1` if none.

### `diku_count_exits`

```c
int diku_count_exits(const room_t *room);
```

Count the number of exits with positive `to_vnum`.

### `diku_sector_name`

```c
const char *diku_sector_name(int sector);
```

Return the sector type name (e.g., `"city"`, `"forest"`). Returns `"unknown"` for out-of-range.

### `diku_item_type_name`

```c
const char *diku_item_type_name(int type);
```

Return the item type name (e.g., `"weapon"`, `"armor"`). Returns `"unknown"` for out-of-range.

### `diku_format_name`

```c
const char *diku_format_name(diku_format_t fmt);
```

Return a human-readable format name (e.g., `"ROM"`, `"SMAUG"`).

---

## Find

**Header:** `diku/find.h`

### `diku_build_vnum_hash`

```c
void diku_build_vnum_hash(area_t *area);
```

Build the `rooms_by_vnum` hash table for an area. Called automatically during parsing; exposed for advanced use.

### `diku_find_room`

```c
room_t *diku_find_room(area_t *area, int vnum);
```

Find a room by vnum within a single area (linear search).

### `diku_find_room_global`

```c
room_t *diku_find_room_global(area_t *areas, int vnum);
```

Find a room by vnum across a linked list of areas.

### `diku_find_mobile`

```c
mobile_t *diku_find_mobile(area_t *area, int vnum);
```

Find a mobile by vnum within a single area.

### `diku_find_item`

```c
item_t *diku_find_item(area_t *area, int vnum);
```

Find an item by vnum within a single area.

### `diku_name_matches`

```c
bool diku_name_matches(const char *name_list, const char *keyword);
```

Case-insensitive word match. `name_list` is a space-separated list of keywords (e.g., `"sword long sharp"`). Returns `true` if any word equals `keyword`.

### `diku_find_room_by_name`

```c
room_t *diku_find_room_by_name(area_t *areas, const char *keyword);
```

Case-insensitive substring search across all room names in all areas. Returns the first match.

### `diku_find_mobile_in_room_by_name`

```c
mobile_t *diku_find_mobile_in_room_by_name(const room_t *room, const char *keyword);
```

Find a mobile in a room's `room_mobiles` list by keyword match.

### `diku_find_item_in_room_by_name`

```c
item_t *diku_find_item_in_room_by_name(const room_t *room, const char *keyword);
```

Find an item in a room's `room_items` list by keyword match.

### `diku_free_area`

```c
void diku_free_area(area_t *area);
```

Free an area and all associated memory (arena, vnum hash table, etc.).

### `diku_free_all_areas`

```c
void diku_free_all_areas(area_t *areas);
```

Free an entire linked list of areas.

---

## Sections

**Header:** `diku/sections.h`

These are low-level section parsers. Most consumers should use the high-level entry points in `format.h` instead.

### `diku_parse_area_header`

```c
area_t *diku_parse_area_header(diku_lexer_t *lex, memento_arena_t *arena);
```

Parse the `#AREA` header and metadata lines. Returns an `area_t` with `arena` set.

### `diku_parse_room`

```c
room_t *diku_parse_room(diku_lexer_t *lex, memento_arena_t *arena, int *vnum_out);
```

Parse a single `#<vnum>` room block. Writes the vnum to `vnum_out`.

### `diku_parse_mobile`

```c
mobile_t *diku_parse_mobile(diku_lexer_t *lex, memento_arena_t *arena, int *vnum_out);
```

Parse a single `#<vnum>` mobile block.

### `diku_parse_item`

```c
item_t *diku_parse_item(diku_lexer_t *lex, memento_arena_t *arena, int *vnum_out);
```

Parse a single `#<vnum>` object block.

### `diku_parse_wld`

```c
bool diku_parse_wld(diku_lexer_t *lex, area_t *area);
```

Parse an entire `#ROOMS` section into `area->rooms`.

### `diku_parse_mob`

```c
bool diku_parse_mob(diku_lexer_t *lex, area_t *area);
```

Parse an entire `#MOBILES` section into `area->mobiles`.

### `diku_parse_obj`

```c
bool diku_parse_obj(diku_lexer_t *lex, area_t *area);
```

Parse an entire `#OBJECTS` section into `area->items`.

### `diku_parse_zon`

```c
bool diku_parse_zon(diku_lexer_t *lex, area_t *area);
```

Parse a `.zon` file into `area->resets_raw` and then invoke `diku_parse_resets()`.

---

## Resets

**Header:** `diku/resets.h`

### `diku_parse_resets`

```c
void diku_parse_resets(area_t *area);
```

Process `area->resets_raw` and populate `room_mobiles`, `room_items`, and mobile `inventory`. Called automatically during parsing.

Supported reset commands:
- `M` — Load mobile into room
- `O` — Load object into room
- `G` — Give object to last mobile
- `E` — Equip object on last mobile
- `P`, `D`, `R`, `S` — Parsed but no action

---

## Format

**Header:** `diku/format.h`

These are the primary entry points for loading area data.

### `diku_parse_file`

```c
area_t *diku_parse_file(diku_context_t *ctx, const char *filename);
```

Parse a single `.are` file. If that fails, falls back to treating the path as a multi-file package base.

**Returns:** The parsed area, or `NULL` on failure. The `ctx` argument provides progress callbacks.

### `diku_parse_lexer`

```c
area_t *diku_parse_lexer(diku_lexer_t *lex, const char *filename);
```

Parse from an already-initialized lexer. Used internally by `diku_parse_file`.

### `diku_parse_package`

```c
area_t *diku_parse_package(diku_context_t *ctx, const char *base_path);
```

Parse a CircleMUD-style multi-file package: `<base_path>.wld`, `.mob`, `.obj`, `.zon`.

### `diku_parse_package_files`

```c
area_t *diku_parse_package_files(const char *wld, const char *mob,
                                  const char *obj, const char *zon);
```

Parse a package from explicit file paths.

### `diku_load_folder_are`

```c
area_t *diku_load_folder_are(diku_context_t *ctx, const char *folder_path);
```

Load all `.are` files in a directory, returning them as a linked list. Invokes the context's progress callback.

**Returns:** Head of the area linked list, or `NULL` if no files loaded.

### `diku_load_folder_packages`

```c
area_t *diku_load_folder_packages(diku_context_t *ctx, const char *folder_path);
```

Load all multi-file packages in a directory (detected by `.wld` files).

### `diku_parse_path`

```c
area_t *diku_parse_path(diku_context_t *ctx, const char *path);
```

Dispatch based on file type: regular file → `diku_parse_file`, directory → `diku_load_folder_are`.

### `diku_detect_format`

```c
diku_format_t diku_detect_format(const area_t *area);
```

Heuristically detect which MUD fork produced the area file. After parsing, the
result is also stored in `area->format`.

---

## Schema / Fork Mapping

**Header:** `diku/schema.h`

`diku/schema.h` exposes a universal schema of canonical constants and conversion
helpers so that flags and values can be interpreted independently of the source
fork.

### Fork enum

```c
typedef enum {
    DIKU_FMT_UNKNOWN = 0,
    DIKU_FMT_DIKU,
    DIKU_FMT_MERC,
    DIKU_FMT_ROM,
    DIKU_FMT_CIRCLE,
    DIKU_FMT_SMAUG,
    DIKU_FMT_CUSTOM
} diku_format_t;
```

### Detection helpers

```c
diku_format_t diku_sniff_format(const char *buf, size_t len);
diku_format_t diku_classify_area(const area_t *area);
```

`diku_sniff_format` performs lightweight pre-parse classification from the raw
file buffer. `diku_classify_area` refines the guess after parsing using populated
`area_t` data.

### Bitvector decoder

```c
uint32_t diku_decode_bitvector(const char *s);   /* A=1<<0, B=1<<1, ... */
uint32_t diku_parse_numeric_flags(const char *s); /* handles 2|512 */
```

### Canonical flag conversions

```c
uint32_t diku_canonical_room_flags(diku_format_t fmt, uint32_t native);
uint32_t diku_canonical_act_flags(diku_format_t fmt, uint32_t native);
uint32_t diku_canonical_affect_flags(diku_format_t fmt, uint32_t native);
uint32_t diku_canonical_wear_flags(diku_format_t fmt, uint32_t native);
uint32_t diku_canonical_extra_flags(diku_format_t fmt, uint32_t native);
```

### Canonical constants

- Room flags: `DIKU_ROOM_DARK`, `DIKU_ROOM_NO_MOB`, `DIKU_ROOM_INDOORS`, ...
- Mobile act flags: `DIKU_ACT_IS_NPC`, `DIKU_ACT_SENTINEL`, ...
- Affect flags: `DIKU_AFF_BLIND`, `DIKU_AFF_INVISIBLE`, ...
- Wear flags: `DIKU_WEAR_TAKE`, `DIKU_WEAR_WIELD`, ...
- Extra flags: `DIKU_EXTRA_GLOW`, `DIKU_EXTRA_HUM`, ...
- Item types: `DIKU_ITEM_WEAPON`, `DIKU_ITEM_ARMOR`, ...
- Sector types: `DIKU_SECT_FOREST`, `DIKU_SECT_CITY`, ...

---

## Graph

**Header:** `diku/graph.h`

### `diku_resolve_graph`

```c
void diku_resolve_graph(area_t **areas, int area_count);
```

Link all `exit_t.to_room` pointers across an array of areas.

### `diku_resolve_graph_global`

```c
void diku_resolve_graph_global(diku_context_t *ctx, area_t *areas);
```

Link all `exit_t.to_room` pointers across a linked list of areas. The `ctx` argument is reserved for future use.

### `diku_check_exit_symmetry`

```c
int diku_check_exit_symmetry(const area_t *area, int *out_total, int *out_asymmetric);
```

Count exits and asymmetric ones (where A→B but B does not have a return exit to A). Returns the asymmetric count. Either output pointer may be `NULL`.

### `diku_print_exit_symmetry`

```c
void diku_print_exit_symmetry(const area_t *area);
```

Print a human-readable symmetry report to `stdout`.

### `diku_graph_diameter`

```c
int diku_graph_diameter(diku_context_t *ctx, area_t *area);
```

Compute the graph diameter (longest shortest path) of an area using BFS. Uses the context's traversal epoch.

---

## Coordinates

**Header:** `diku/coords.h`

### `diku_find_central_room`

```c
room_t *diku_find_central_room(area_t *area);
```

Heuristic to find a good root room for coordinate assignment. Prefers the room with the highest degree (most exits); falls back to the room closest to the midpoint vnum.

### `diku_assign_coordinates`

```c
void diku_assign_coordinates(diku_context_t *ctx, area_t *area, room_t *root);
```

Assign 3D coordinates to all rooms in an area via BFS starting from `root`. Uses collision detection with spiral search. Stores bounds in `ctx->coord_bounds`.

### `diku_assign_coordinates_all`

```c
void diku_assign_coordinates_all(diku_context_t *ctx, area_t *areas);
```

Assign coordinates to each area in a linked list independently, using `diku_find_central_room()` for each.

### `diku_assign_coordinates_multi`

```c
void diku_assign_coordinates_multi(diku_context_t *ctx, area_t *areas);
```

Assign coordinates across all areas in a linked list as a single connected graph.

### `diku_room_at_coord`

```c
room_t *diku_room_at_coord(area_t *area, int x, int y, int z);
```

Find the room at the given coordinates (linear search).

### `diku_coord_occupied`

```c
bool diku_coord_occupied(area_t *area, int x, int y, int z, room_t *exclude);
```

Check if any room (other than `exclude`) occupies the given coordinates.

### `diku_center_coordinates`

```c
void diku_center_coordinates(area_t *area);
```

Shift all assigned coordinates so their centroid is at the origin.

### `diku_get_coord_bounds`

```c
void diku_get_coord_bounds(area_t *area, int *min_x, int *max_x,
                            int *min_y, int *max_y, int *min_z, int *max_z);
```

Compute the bounding box of assigned coordinates.

---

## Statistics

**Header:** `diku/stats.h`

### `diku_totals_t`

```c
typedef struct {
    int areas;
    int rooms;
    int mobiles;
    int items;
    int exits;
    int resolved_exits;
    int sym_total;
    int sym_asymmetric;
} diku_totals_t;
```

### `diku_compute_totals`

```c
void diku_compute_totals(area_t *areas, diku_totals_t *out);
```

Aggregate counts across an entire area list.

### `diku_print_totals`

```c
void diku_print_totals(FILE *fp, const diku_totals_t *t,
                        const char *source, area_t *areas);
```

Print a formatted overview table to `fp`, including per-area breakdowns.

---

## Debug

**Header:** `diku/debug.h`

All `*_fp` variants write to an arbitrary `FILE*`. The non-`_fp` variants are `static inline` wrappers that write to `stdout`.

### `diku_print_room_fp`

```c
void diku_print_room_fp(FILE *fp, const room_t *room);
```

Print room details: vnum, name, description, flags, sector, exits, coordinates.

### `diku_print_mobile_fp`

```c
void diku_print_mobile_fp(FILE *fp, const mobile_t *mob);
```

Print mobile details: vnum, names, stats, AC, gold.

### `diku_print_item_fp`

```c
void diku_print_item_fp(FILE *fp, const item_t *item);
```

Print item details: vnum, names, type, values, flags.

### `diku_print_area_info_fp`

```c
void diku_print_area_info_fp(FILE *fp, const area_t *area);
```

Print area metadata: name, file, builders, counts.

### `diku_print_graph_fp`

```c
void diku_print_graph_fp(FILE *fp, const area_t *area);
```

Print all rooms in the area.

### `diku_print_coordinates_fp`

```c
void diku_print_coordinates_fp(FILE *fp, area_t *area);
```

Print a 2D grid map for each Z level, showing vnums at each coordinate.

---

## Pager

**Header:** `diku/pager.h`

### `diku_page_action_t`

```c
typedef enum {
    DIKU_PAGE_NEXT,   /* continue to next item */
    DIKU_PAGE_QUIT,   /* stop paging entirely */
    DIKU_PAGE_SKIP    /* skip to next category */
} diku_page_action_t;
```

### `diku_pager_render_fn`

```c
typedef void (*diku_pager_render_fn)(FILE *fp, const void *item, void *user);
```

Callback to render one item to a `FILE*`.

### `diku_pager_prompt_fn`

```c
typedef diku_page_action_t (*diku_pager_prompt_fn)(const char *category,
                                                    int cur, int total, void *user);
```

Callback to prompt the user and return an action. If `NULL`, `diku_pager_default_prompt` is used.

### `diku_pager_t`

```c
typedef struct diku_pager {
    const char            *category;
    FILE                  *out;
    diku_pager_render_fn   render;
    diku_pager_prompt_fn   prompt;
    void                  *user;
} diku_pager_t;
```

### `diku_pager_default_prompt`

```c
diku_page_action_t diku_pager_default_prompt(const char *category,
                                              int cur, int total, void *user);
```

Default prompt reading from `stdin`. Accepts `q` to quit, `n` to skip category, Enter to continue.

### `diku_pager_run_arealist_rooms`

```c
bool diku_pager_run_arealist_rooms(diku_pager_t *p, area_t *areas);
```

Page through all rooms in all areas. Returns `true` if the user quit, `false` otherwise (including skip).

### `diku_pager_run_arealist_mobiles`

```c
bool diku_pager_run_arealist_mobiles(diku_pager_t *p, area_t *areas);
```

Page through all mobiles.

### `diku_pager_run_arealist_items`

```c
bool diku_pager_run_arealist_items(diku_pager_t *p, area_t *areas);
```

Page through all items.

---

## C++ Wrapper

**Header:** `diku.hpp`

The C++ wrapper provides RAII classes, `std::string_view` integration, and forward iterators over the C data structures. All functions are inline and header-only.

### `sv()` helper

```cpp
inline std::string_view sv(const diku_string_t& s) noexcept;
```

Convert a `diku_string_t` to `std::string_view`.

### `Context`

```cpp
class Context {
public:
    Context();
    explicit Context(diku_context_t* raw);

    diku_context_t* get() const noexcept;
    diku_context_t* operator->() const noexcept;
    explicit operator bool() const noexcept;

    void set_progress(diku_progress_cb_t cb, void* user = nullptr) noexcept;
    void set_verbose(bool verbose = true) noexcept;
};
```

RAII wrapper around `diku_context_t`. Uses `std::unique_ptr` with a custom deleter.

### `ExitRef`

```cpp
class ExitRef {
public:
    explicit ExitRef(exit_t* p) noexcept;

    int direction() const noexcept;
    std::string_view desc() const noexcept;
    std::string_view keywords() const noexcept;
    int to_vnum() const noexcept;
    room_t* to_room() const noexcept;
};
```

Non-owning reference to an `exit_t`.

### `RoomRef`

```cpp
class RoomRef {
public:
    explicit RoomRef(room_t* p) noexcept;

    int vnum() const noexcept;
    std::string_view name() const noexcept;
    std::string_view desc() const noexcept;
    int sector() const noexcept;
    const coord3d_t& coord() const noexcept;
    bool coord_assigned() const noexcept;

    ExitRef exit(int dir) const noexcept;
    int extra_desc_count() const noexcept;
    int room_mobile_count() const noexcept;
    int room_item_count() const noexcept;
};
```

Non-owning reference to a `room_t`.

### `MobileRef`

```cpp
class MobileRef {
public:
    explicit MobileRef(mobile_t* p) noexcept;

    int vnum() const noexcept;
    std::string_view name() const noexcept;
    std::string_view short_desc() const noexcept;
    std::string_view long_desc() const noexcept;
    int level() const noexcept;
};
```

Non-owning reference to a `mobile_t`.

### `ItemRef`

```cpp
class ItemRef {
public:
    explicit ItemRef(item_t* p) noexcept;

    int vnum() const noexcept;
    std::string_view name() const noexcept;
    std::string_view short_desc() const noexcept;
    int type() const noexcept;
    int weight() const noexcept;
    int cost() const noexcept;
};
```

Non-owning reference to an `item_t`.

### `Area`

```cpp
class Area {
public:
    Area() noexcept;
    explicit Area(area_t* p) noexcept;

    std::string_view name() const noexcept;
    std::string_view filename() const noexcept;
    std::string_view builders() const noexcept;

    int room_count() const noexcept;
    int mobile_count() const noexcept;
    int item_count() const noexcept;

    Area next() const noexcept;

    RoomRef room(int idx) const noexcept;
    RoomRef room_by_vnum(int vnum) const noexcept;
    MobileRef mobile_by_vnum(int vnum) const noexcept;
    ItemRef item_by_vnum(int vnum) const noexcept;

    RoomRef rooms_begin() const noexcept;
    RoomRef rooms_end() const noexcept;
    MobileRef mobiles_begin() const noexcept;
    MobileRef mobiles_end() const noexcept;
    ItemRef items_begin() const noexcept;
    ItemRef items_end() const noexcept;
};
```

Non-owning view of an `area_t`. Provides index-based access and C++ iterator-style `begin()`/`end()` pairs for range-based for loops.

**Note:** `Area` itself is not a C++ container iterator. Use `AreaList` to iterate over multiple areas.

### `AreaList`

```cpp
class AreaList {
public:
    AreaList() noexcept;
    explicit AreaList(area_t* head) noexcept;

    AreaList(AreaList&& other) noexcept;
    AreaList& operator=(AreaList&& other) noexcept;
    ~AreaList();

    AreaList(const AreaList&) = delete;
    AreaList& operator=(const AreaList&) = delete;

    area_t* head() const noexcept;
    bool empty() const noexcept;
    explicit operator bool() const noexcept;
    area_t* release() noexcept;

    class iterator { /* forward_iterator_tag */ };

    iterator begin() const noexcept;
    iterator end() const noexcept;
};
```

Owning wrapper for a linked list of `area_t`. Move-only. The destructor calls `diku_free_all_areas()`.

Supports range-based for:

```cpp
for (const auto& area : areas) { /* area is diku::Area */ }
```

### Parsing Functions

```cpp
AreaList parse_file(Context& ctx, const char* path);
AreaList parse_lexer(Context& ctx, diku_lexer_t& lex, const char* filename);
AreaList parse_package(Context& ctx, const char* base_path);
AreaList parse_path(Context& ctx, const char* path);
AreaList load_folder_are(Context& ctx, const char* folder_path);
AreaList load_folder_packages(Context& ctx, const char* folder_path);
```

### Graph & Coordinates

```cpp
void resolve_graph(Context& ctx, AreaList& areas);
void assign_coordinates(Context& ctx, Area& area, RoomRef root);
void assign_coordinates_all(Context& ctx, AreaList& areas);
void assign_coordinates_multi(Context& ctx, AreaList& areas);
RoomRef find_central_room(Area& area);
void center_coordinates(Area& area);
void get_coord_bounds(Area& area, int& min_x, int& max_x,
                      int& min_y, int& max_y, int& min_z, int& max_z);
```

### Statistics & Debug

```cpp
void compute_totals(AreaList& areas, diku_totals_t& out);
void print_totals(FILE* fp, const diku_totals_t& t,
                  std::string_view source, AreaList& areas);

void print_room_fp(FILE* fp, RoomRef room);
void print_mobile_fp(FILE* fp, MobileRef mob);
void print_item_fp(FILE* fp, ItemRef item);
void print_area_info_fp(FILE* fp, Area area);
void print_graph_fp(FILE* fp, Area area);
void print_coordinates_fp(FILE* fp, Area area);
```

### Topology & Utilities

```cpp
int graph_diameter(Context& ctx, Area& area);
int check_exit_symmetry(Area& area, int& total, int& asymmetric);
void print_exit_symmetry(FILE* fp, Area& area);
diku_format_t detect_format(Area& area);
```
