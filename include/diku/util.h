#ifndef DIKU_UTIL_H
#define DIKU_UTIL_H

#include "diku/types.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline const char *diku_dir_name(int dir) {
    static const char *names[] = {
        "north", "east", "south", "west", "up", "down",
        "northeast", "northwest", "southeast", "southwest",
        "in", "out"
    };
    if (dir < 0 || dir >= DIKU_MAX_EXITS) return "unknown";
    return names[dir];
}

static inline const char *diku_dir_name_short(int dir) {
    static const char *names[] = {
        "n", "e", "s", "w", "u", "d",
        "ne", "nw", "se", "sw", "in", "out"
    };
    if (dir < 0 || dir >= DIKU_MAX_EXITS) return "?";
    return names[dir];
}

static inline int diku_reverse_dir(int dir) {
    static const int reverse[] = {
        DIR_SOUTH, DIR_WEST, DIR_NORTH, DIR_EAST, DIR_DOWN, DIR_UP,
        DIR_SOUTHWEST, DIR_SOUTHEAST, DIR_NORTHWEST, DIR_NORTHEAST,
        DIR_OUT, DIR_IN
    };
    if (dir < 0 || dir >= DIKU_MAX_EXITS) return -1;
    return reverse[dir];
}

static inline bool diku_has_exit(const room_t *room, int dir) {
    if (!room || dir < 0 || dir >= DIKU_MAX_EXITS) return false;
    return room->exits[dir] != NULL && room->exits[dir]->to_vnum > 0;
}

static inline int diku_get_exit_vnum(const room_t *room, int dir) {
    if (!diku_has_exit(room, dir)) return -1;
    return room->exits[dir]->to_vnum;
}

static inline int diku_count_exits(const room_t *room) {
    if (!room) return 0;
    int count = 0;
    for (int i = 0; i < DIKU_MAX_EXITS; i++) {
        if (room->exits[i] && room->exits[i]->to_vnum > 0)
            count++;
    }
    return count;
}

static inline const char *diku_sector_name(int sector) {
    static const char *names[] = {
        "inside", "city", "field", "forest", "hills", "mountain",
        "water_swim", "water_noswim", "underwater", "air", "desert",
        "unknown", "unknown", "unknown", "unknown", "unknown"
    };
    if (sector < 0 || sector >= 16) return "unknown";
    return names[sector];
}

static inline const char *diku_item_type_name(int type) {
    static const char *names[] = {
        "none", "light", "scroll", "wand", "staff", "weapon",
        "unused", "unused", "treasure", "armor", "potion",
        "unused", "furniture", "trash", "unused", "container",
        "unused", "drink", "key", "food", "money", "unused",
        "boat", "corpse_npc", "corpse_pc", "fountain", "pill",
        "protect", "map", "portal", "warp_stone", "room_key",
        "gem", "jewelry", "jitbag"
    };
    if (type < 0 || type >= 35) return "unknown";
    return names[type];
}

static inline const char *diku_format_name(diku_format_t fmt) {
    switch (fmt) {
        case DIKU_FMT_DIKU:    return "DikuMUD";
        case DIKU_FMT_MERC:    return "Merc";
        case DIKU_FMT_ROM:     return "ROM";
        case DIKU_FMT_CIRCLE:  return "CircleMUD";
        case DIKU_FMT_SMAUG:   return "SMAUG";
        case DIKU_FMT_CUSTOM:  return "Custom";
        default:               return "Unknown";
    }
}

#ifdef __cplusplus
}
#endif
#endif /* DIKU_UTIL_H */
