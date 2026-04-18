#ifndef DIKU_RESETS_H
#define DIKU_RESETS_H

#include "diku/find.h"

#ifdef __cplusplus
extern "C" {
#endif

void diku_parse_resets(area_t *area);

#ifdef DIKU_PARSER_IMPLEMENTATION

void diku_parse_resets(area_t *area) {
    if (!area || !area->resets_raw || area->resets_line_count <= 0) return;

    mobile_t *last_mob = NULL;

    for (int i = 0; i < area->resets_line_count; i++) {
        char *line = area->resets_raw[i];
        if (!line || !line[0]) continue;

        char cmd;
        int arg1, arg2, arg3, arg4, arg5;
        int n = sscanf(line, " %c %d %d %d %d %d", &cmd, &arg1, &arg2, &arg3, &arg4, &arg5);
        if (n < 2) continue;

        switch (cmd) {
            case 'M':
            case 'm': {
                if (n < 5) break;
                int mob_vnum = arg2;
                int room_vnum = arg4;
                mobile_t *mob = diku_find_mobile(area, mob_vnum);
                room_t *room = diku_find_room(area, room_vnum);
                if (mob && room) {
                    int idx = room->room_mobile_count++;
                    size_t new_size = room->room_mobile_count * sizeof(mobile_t *);
                    mobile_t **new_array = (mobile_t **)diku_arena_alloc(area->arena, new_size);
                    if (new_array && room->room_mobiles)
                        memcpy(new_array, room->room_mobiles, idx * sizeof(mobile_t *));
                    room->room_mobiles = new_array;
                    if (room->room_mobiles)
                        room->room_mobiles[idx] = mob;
                    last_mob = mob;
                }
                break;
            }

            case 'O':
            case 'o': {
                if (n < 5) break;
                int obj_vnum = arg2;
                int room_vnum = arg4;
                item_t *item = diku_find_item(area, obj_vnum);
                room_t *room = diku_find_room(area, room_vnum);
                if (item && room) {
                    int idx = room->room_item_count++;
                    size_t new_size = room->room_item_count * sizeof(item_t *);
                    item_t **new_array = (item_t **)diku_arena_alloc(area->arena, new_size);
                    if (new_array && room->room_items)
                        memcpy(new_array, room->room_items, idx * sizeof(item_t *));
                    room->room_items = new_array;
                    if (room->room_items)
                        room->room_items[idx] = item;
                }
                break;
            }

            case 'G':
            case 'g': {
                if (n < 4 || !last_mob) break;
                int obj_vnum = arg2;
                item_t *item = diku_find_item(area, obj_vnum);
                if (item && last_mob) {
                    int idx = last_mob->inventory_count++;
                    size_t new_size = last_mob->inventory_count * sizeof(*last_mob->inventory);
                    void *new_array = diku_arena_alloc(area->arena, new_size);
                    if (new_array && last_mob->inventory)
                        memcpy(new_array, last_mob->inventory, idx * sizeof(*last_mob->inventory));
                    last_mob->inventory = new_array;
                    if (last_mob->inventory) {
                        last_mob->inventory[idx].item = item;
                        last_mob->inventory[idx].wear_loc = -1;
                    }
                }
                break;
            }

            case 'E':
            case 'e': {
                if (n < 5 || !last_mob) break;
                int obj_vnum = arg2;
                int wear_loc = arg4;
                item_t *item = diku_find_item(area, obj_vnum);
                if (item && last_mob) {
                    int idx = last_mob->inventory_count++;
                    size_t new_size = last_mob->inventory_count * sizeof(*last_mob->inventory);
                    void *new_array = diku_arena_alloc(area->arena, new_size);
                    if (new_array && last_mob->inventory)
                        memcpy(new_array, last_mob->inventory, idx * sizeof(*last_mob->inventory));
                    last_mob->inventory = new_array;
                    if (last_mob->inventory) {
                        last_mob->inventory[idx].item = item;
                        last_mob->inventory[idx].wear_loc = wear_loc;
                    }
                }
                break;
            }

            case 'P': case 'p':
            case 'D': case 'd':
            case 'R': case 'r':
            case 'S': case 's':
            default:
                break;
        }
    }
}

#endif /* DIKU_PARSER_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif
#endif /* DIKU_RESETS_H */
