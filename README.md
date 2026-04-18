# libdiku — DikuMUD Area Parser

A robust C11 parser for DikuMUD, Merc, ROM, and SMAUG area files, with a C++17 convenience wrapper. Features zero-global-state design, arena-based memory management, graph resolution, and 3D coordinate assignment.

## Features

- **Multi-format support**: Handles DikuMUD, Merc, ROM, CircleMUD, and SMAUG area formats
- **Modular header-only C library**: 16 focused headers under `include/diku/`, umbrella `include/diku.h`
- **Zero mutable globals**: All state lives in an explicit `diku_context_t` — fully thread-safe
- **Arena allocator**: Fast, cache-friendly memory management with 4 MiB slabs via Memento
- **Graph resolution**: Links exits to room pointers across single or multiple areas
- **3D coordinate assignment**: Best-effort positioning with collision detection and spiral search
- **C++17 wrapper**: RAII `diku::Context`, `diku::AreaList`, `diku::RoomRef`, etc.
- **Fork-friendly**: Stores unknown sections/raw data for extension

## Building

```bash
mkdir build && cd build
cmake ..
make -j
```

CMake automatically fetches the **Memento** allocator via FetchContent.

### Build Targets

| Target | Output | Description |
|--------|--------|-------------|
| `diku_parser` | INTERFACE | Include paths + link deps for consumers |
| `diku_impl` | `libdiku_impl.a` | Single static TU with all implementations |
| `diku_tool` | `test_parser` | CLI driver for inspecting area files |
| `mud_client` | `mud_client` | Interactive MUD client |
| `test_parser_comprehensive` | — | Stress test over all `.are` files |

### Header-Only Consumption

Skip the static library and compile the implementation into your own TU:

```c
#define MEMENTO_IMPLEMENTATION
#define DIKU_PARSER_IMPLEMENTATION
#include "diku.h"
```

## Quick Start (C)

```c
#include "diku.h"

int main() {
    diku_context_t *ctx = diku_context_create();
    if (!ctx) return 1;

    // Parse an area file, folder, or multi-file package
    area_t *areas = diku_parse_path(ctx, "myarea.are");
    if (!areas) {
        perror("Failed to parse area");
        diku_context_destroy(ctx);
        return 1;
    }

    // Resolve exit pointers across all loaded areas
    diku_resolve_graph_global(ctx, areas);

    // Assign 3D coordinates (finds central room automatically)
    diku_assign_coordinates_all(ctx, areas);

    // Use the data...
    for (area_t *area = areas; area; area = area->next) {
        for (int i = 0; i < area->room_count; i++) {
            room_t *room = &area->rooms[i];
            printf("Room #%d: %s at (%d, %d, %d)\n",
                   room->vnum, room->name.str,
                   room->coord.x, room->coord.y, room->coord.z);
        }
    }

    // Cleanup — one call frees an entire area and everything inside it
    diku_free_all_areas(areas);
    diku_context_destroy(ctx);
    return 0;
}
```

## Quick Start (C++)

```cpp
#include "diku.hpp"
#include <iostream>

int main() {
    diku::Context ctx;

    auto areas = diku::parse_path(ctx, "data/haon.are");
    if (areas.empty()) {
        std::cerr << "Failed to load areas\n";
        return 1;
    }

    diku::resolve_graph(ctx, areas);
    diku::assign_coordinates_all(ctx, areas);

    for (const auto& area : areas) {
        std::cout << "Area: " << area.name()
                  << " rooms=" << area.room_count() << "\n";
        for (auto r = area.rooms_begin(); r != area.rooms_end(); ++r) {
            auto room = *r;
            std::cout << "  Room " << room.vnum()
                      << " " << room.name()
                      << " at (" << room.coord().x << ","
                      << room.coord().y << ","
                      << room.coord().z << ")\n";
        }
    }
    return 0;
}
```

## CLI Usage (`test_parser`)

```bash
./test_parser <path> [options]

Path can be:
  <area_file.are>     A single area file
  <package_base>      Base path to a classic .wld/.mob/.obj/.zon package
  <directory>         Load all .are files (or packages with --packages)

Options:
  -v, --verbose       Page through selected entities interactively
  --all               Select all entities (default with -v)
  --rooms             Print all rooms
  --mobiles           Print all mobiles
  --objects           Print all objects/items
  --graph             Print graph connections
  --coords            Print 3D coordinates
  --symmetry          Show exit symmetry report
  --packages          When loading a directory, load multi-file packages
  -h, --help          Show this help

Without -v, an overview summary is printed.
```

## Architecture

The library is organized into 16 focused headers under `include/diku/`:

| Header | Responsibility |
|--------|---------------|
| `config.h` | Constants: `DIKU_MAX_EXITS`, direction enums, `dir_offset[]` |
| `types.h` | POD structs: `room_t`, `mobile_t`, `item_t`, `area_t`, `diku_lexer_t` |
| `arena.h` | Memento wrappers: `diku_arena_create`, `diku_arena_strdup`, etc. |
| `context.h` | `diku_context_t` — per-operation state (replaces all globals) |
| `lexer.h` | Self-contained tokenizer over memory-backed buffer |
| `util.h` | `static inline` pure helpers: `diku_dir_name`, `diku_sector_name`, etc. |
| `find.h` | Vnum hash builder + lookups by vnum, by name, by contents |
| `sections.h` | Section parsers: rooms, mobiles, items, area header |
| `resets.h` | Reset command parsing (`M`, `O`, `G`, `E`, etc.) |
| `format.h` | Entry points: `diku_parse_file`, `diku_parse_path`, `diku_load_folder_are` |
| `graph.h` | Graph resolution, exit symmetry, graph diameter |
| `coords.h` | 3D coordinate assignment with per-context collision hash |
| `stats.h` | `diku_compute_totals`, `diku_print_totals` |
| `debug.h` | Pretty-printers to `FILE*` sink |
| `pager.h` | Generic pagination with render/prompt callbacks |
| `diku.h` | Umbrella — includes all sub-headers in dependency order |

## Data Structures

### Core Types

```c
typedef struct exit_t {
    int direction;           /* 0=N, 1=E, 2=S, 3=W, 4=U, 5=D, 6=NE... */
    diku_string_t desc;
    diku_string_t keywords;
    int flags;
    int key_vnum;
    int to_vnum;             /* before resolution */
    room_t *to_room;         /* after diku_resolve_graph() */
    exit_t *next;            /* for extra dirs if >6 */
} exit_t;

typedef struct room_t {
    int vnum;
    diku_string_t name;
    diku_string_t desc;
    int flags;
    int sector;
    exit_t *exits[DIKU_MAX_EXITS];
    coord3d_t coord;         /* 3D position */
    bool coord_assigned;
    uint32_t traversal_epoch;
    int traversal_dist;
    mobile_t **room_mobiles;
    int room_mobile_count;
    item_t **room_items;
    int room_item_count;
    void *user;              /* hook for your data */
} room_t;

typedef struct area_t {
    diku_string_t name, filename, builders, credits;
    int low_level, high_level, low_vnum, high_vnum;
    room_t *rooms; int room_count;
    room_t **rooms_by_vnum;  /* vnum hash table */
    mobile_t *mobiles; int mobile_count;
    item_t *items; int item_count;
    arena_t *arena;          /* all memory lives here */
    area_t *next;            /* linked list for multi-area loads */
} area_t;
```

## 3D Coordinate System

The parser assigns 3D coordinates using a BFS traversal:
- **North**: +Y
- **East**: +X
- **Up**: +Z
- **Down**: -Z

Central room detection uses degree centrality (most connected room) with vnum midpoint as fallback. Collision detection uses a hash table with spiral search for alternative positions.

## Fork Compatibility

The parser handles common Diku variants:
- **DikuMUD**: Original format
- **Merc**: Extended stats, bitvector flags
- **ROM**: Materials, levels, complex mobs
- **SMAUG**: Security levels, version info
- **CircleMUD**: Multi-file package format (`.wld`/`.mob`/`.obj`/`.zon`)

Unknown header lines and sections are stored raw for fork-specific handling.

## Backward Compatibility

`#include "diku_parser.h"` continues to work — it simply redirects to `diku.h`.

## License

Public domain — use as you wish.
