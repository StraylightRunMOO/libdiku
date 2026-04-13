# DikuMUD Area Parser

A robust C parser for DikuMUD, Merc, ROM, and compatible area files. Features arena-based memory management, graph resolution, and 3D coordinate assignment.

## Features

- **Multi-format support**: Handles DikuMUD, Merc, ROM, and SMAUG area formats
- **Arena allocator**: Fast, cache-friendly memory management with 4 MiB slabs
- **Graph resolution**: Links exits to room pointers across multiple areas
- **3D coordinate assignment**: Best-effort positioning with collision detection
- **Fork-friendly**: Stores unknown sections/raw data for extension

## Building

```bash
gcc -O2 -o diku_parser diku_parser.c test_parser.c -lm
```

Or include in your project:
```c
#include "diku_parser.h"
```

## Quick Start

```c
#include "diku_parser.h"

int main() {
    // Parse an area file
    area_t *area = diku_parse_file("myarea.are");
    if (!area) {
        perror("Failed to parse area");
        return 1;
    }
    
    // Resolve exit pointers
    diku_resolve_graph_global(area);
    
    // Assign 3D coordinates (finds central room automatically)
    diku_assign_coordinates_all(area);
    
    // Use the data...
    for (int i = 0; i < area->room_count; i++) {
        room_t *room = &area->rooms[i];
        printf("Room #%d: %s at (%d, %d, %d)\n",
               room->vnum, room->name.str,
               room->coord.x, room->coord.y, room->coord.z);
    }
    
    // Cleanup
    diku_free_area(area);
    return 0;
}
```

## Test Program

```bash
./test_parser <area_file> [options]

Options:
  -i, --info      Print area info (default)
  -r, --rooms     Print all rooms
  -m, --mobiles   Print all mobiles
  -o, --objects   Print all objects/items
  -g, --graph     Print graph connections
  -c, --coords    Print 3D coordinates
  -a, --all       Print everything
```

## Data Structures

### Core Types

```c
typedef struct exit_t {
    int direction;           /* 0=N, 1=E, 2=S, 3=W, 4=U, 5=D */
    diku_string_t desc;
    diku_string_t keywords;
    int flags;
    int key_vnum;
    int to_vnum;             /* before resolution */
    room_t *to_room;         /* after resolve_graph() */
    exit_t *next;            /* for extra dirs if >6 */
} exit_t;

typedef struct room_t {
    int vnum;
    diku_string_t name;
    diku_string_t desc;
    int flags;
    int sector;
    exit_t *exits[6];
    coord3d_t coord;         /* 3D position */
    bool coord_assigned;
    void *user;              /* hook for your data */
} room_t;

typedef struct area_t {
    diku_string_t name;
    diku_string_t builders;
    int low_vnum, high_vnum;
    
    room_t *rooms;
    int room_count;
    
    mobile_t *mobiles;
    int mobile_count;
    
    item_t *items;
    int item_count;
    
    arena_t *arena;          /* all memory lives here */
} area_t;
```

## API Reference

### Parsing
- `area_t *diku_parse_file(const char *filename)` - Parse an area file
- `area_t *diku_parse_fp(FILE *fp, const char *filename)` - Parse from FILE*

### Graph Resolution
- `void diku_resolve_graph(area_t **areas, int area_count)` - Link exits across areas
- `void diku_resolve_graph_global(area_t *areas)` - Link exits in single area list

### 3D Coordinates
- `room_t *diku_find_central_room(area_t *area)` - Find central room (heuristic)
- `void diku_assign_coordinates(area_t *area, room_t *root)` - Assign from root
- `void diku_assign_coordinates_all(area_t *areas)` - Auto-assign for all areas
- `void diku_center_coordinates(area_t *area)` - Shift to center around origin
- `void diku_get_coord_bounds(area_t *area, ...)` - Get coordinate bounds

### Lookup
- `room_t *diku_find_room(area_t *area, int vnum)` - Find room by vnum
- `room_t *diku_find_room_global(area_t *areas, int vnum)` - Find across areas
- `mobile_t *diku_find_mobile(area_t *area, int vnum)` - Find mobile by vnum
- `item_t *diku_find_item(area_t *area, int vnum)` - Find item by vnum

### Utilities
- `const char *diku_dir_name(int dir)` - "north", "east", etc.
- `const char *diku_sector_name(int sector)` - "city", "forest", etc.
- `const char *diku_item_type_name(int type)` - "weapon", "armor", etc.
- `int diku_graph_diameter(area_t *area)` - Calculate graph diameter

### Cleanup
- `void diku_free_area(area_t *area)` - Free an area
- `void diku_free_all_areas(area_t *areas)` - Free area list

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

Unknown header lines and sections are stored raw for fork-specific handling.

## License

Public domain - use as you wish.
