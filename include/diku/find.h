#ifndef DIKU_FIND_H
#define DIKU_FIND_H

#include <strings.h>
#include "diku/types.h"

#ifdef __cplusplus
extern "C" {
#endif

void      diku_build_vnum_hash(area_t *area);
room_t   *diku_find_room(area_t *area, int vnum);
room_t   *diku_find_room_global(area_t *areas, int vnum);
mobile_t *diku_find_mobile(area_t *area, int vnum);
item_t   *diku_find_item(area_t *area, int vnum);

bool      diku_name_matches(const char *name_list, const char *keyword);
room_t   *diku_find_room_by_name(area_t *areas, const char *keyword);
mobile_t *diku_find_mobile_in_room_by_name(const room_t *room, const char *keyword);
item_t   *diku_find_item_in_room_by_name(const room_t *room, const char *keyword);

#ifdef DIKU_PARSER_IMPLEMENTATION

void diku_build_vnum_hash(area_t *area) {
    if (!area || area->room_count == 0) return;
    if (area->rooms_by_vnum) {
        free(area->rooms_by_vnum);
        area->rooms_by_vnum = NULL;
    }
    area->rooms_by_vnum = (room_t **)calloc(DIKU_VNUM_HASH_SIZE, sizeof(room_t *));
    if (!area->rooms_by_vnum) return;
    for (int i = 0; i < area->room_count; i++) {
        int hash = area->rooms[i].vnum & DIKU_VNUM_HASH_MASK;
        if (!area->rooms_by_vnum[hash])
            area->rooms_by_vnum[hash] = &area->rooms[i];
    }
}

room_t *diku_find_room(area_t *area, int vnum) {
    if (!area) return NULL;
    for (int i = 0; i < area->room_count; i++) {
        if (area->rooms[i].vnum == vnum)
            return &area->rooms[i];
    }
    return NULL;
}

room_t *diku_find_room_global(area_t *areas, int vnum) {
    for (area_t *area = areas; area; area = area->next) {
        room_t *room = diku_find_room(area, vnum);
        if (room) return room;
    }
    return NULL;
}

mobile_t *diku_find_mobile(area_t *area, int vnum) {
    if (!area) return NULL;
    for (int i = 0; i < area->mobile_count; i++) {
        if (area->mobiles[i].vnum == vnum)
            return &area->mobiles[i];
    }
    return NULL;
}

item_t *diku_find_item(area_t *area, int vnum) {
    if (!area) return NULL;
    for (int i = 0; i < area->item_count; i++) {
        if (area->items[i].vnum == vnum)
            return &area->items[i];
    }
    return NULL;
}

bool diku_name_matches(const char *name_list, const char *keyword) {
    if (!name_list || !keyword) return false;
    size_t klen = strlen(keyword);
    const char *p = name_list;
    while (*p) {
        while (*p == ' ') p++;
        const char *word_start = p;
        while (*p && *p != ' ') p++;
        size_t wlen = (size_t)(p - word_start);
        if (wlen == klen && strncasecmp(word_start, keyword, klen) == 0)
            return true;
    }
    return false;
}

room_t *diku_find_room_by_name(area_t *areas, const char *keyword) {
    if (!keyword) return NULL;
    size_t klen = strlen(keyword);
    for (area_t *area = areas; area; area = area->next) {
        for (int i = 0; i < area->room_count; i++) {
            room_t *room = &area->rooms[i];
            if (!room->name.str || room->name.len < klen) continue;
            /* Case-insensitive substring search */
            for (size_t j = 0; j + klen <= room->name.len; j++) {
                if (strncasecmp(room->name.str + j, keyword, klen) == 0)
                    goto found;
            }
            continue;
            found: return room;
        }
    }
    return NULL;
}

mobile_t *diku_find_mobile_in_room_by_name(const room_t *room, const char *keyword) {
    if (!room || !keyword) return NULL;
    for (int i = 0; i < room->room_mobile_count; i++) {
        mobile_t *mob = room->room_mobiles[i];
        if (mob && mob->name.str && diku_name_matches(mob->name.str, keyword))
            return mob;
    }
    return NULL;
}

item_t *diku_find_item_in_room_by_name(const room_t *room, const char *keyword) {
    if (!room || !keyword) return NULL;
    for (int i = 0; i < room->room_item_count; i++) {
        item_t *item = room->room_items[i];
        if (item && item->name.str && diku_name_matches(item->name.str, keyword))
            return item;
    }
    return NULL;
}

#endif /* DIKU_PARSER_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif
#endif /* DIKU_FIND_H */
