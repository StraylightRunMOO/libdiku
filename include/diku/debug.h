#ifndef DIKU_DEBUG_H
#define DIKU_DEBUG_H

#include "diku/util.h"

/* Forward declarations from coords.h (not yet extracted) */
void diku_get_coord_bounds(area_t *area, int *min_x, int *max_x, int *min_y, int *max_y, int *min_z, int *max_z);
room_t *diku_room_at_coord(area_t *area, int x, int y, int z);

#ifdef __cplusplus
extern "C" {
#endif

void diku_print_room_fp(FILE *fp, const room_t *room);
void diku_print_mobile_fp(FILE *fp, const mobile_t *mob);
void diku_print_item_fp(FILE *fp, const item_t *item);
void diku_print_area_info_fp(FILE *fp, const area_t *area);
void diku_print_graph_fp(FILE *fp, const area_t *area);
void diku_print_coordinates_fp(FILE *fp, area_t *area);

static inline void diku_print_room(const room_t *room) { diku_print_room_fp(stdout, room); }
static inline void diku_print_mobile(const mobile_t *mob) { diku_print_mobile_fp(stdout, mob); }
static inline void diku_print_item(const item_t *item) { diku_print_item_fp(stdout, item); }
static inline void diku_print_area_info(const area_t *area) { diku_print_area_info_fp(stdout, area); }
static inline void diku_print_graph(const area_t *area) { diku_print_graph_fp(stdout, area); }
static inline void diku_print_coordinates(area_t *area) { diku_print_coordinates_fp(stdout, area); }

#ifdef DIKU_PARSER_IMPLEMENTATION

void diku_print_room_fp(FILE *fp, const room_t *room) {
    if (!room) {
        fprintf(fp, "  (null room)\n");
        return;
    }
    fprintf(fp, "  Room #%d: %s\n", room->vnum, room->name.str ? room->name.str : "(unnamed)");
    fprintf(fp, "    Desc: %.60s%s\n",
            room->desc.str ? room->desc.str : "(none)",
            room->desc.len > 60 ? "..." : "");
    fprintf(fp, "    Flags: 0x%x, Sector: %d\n", room->flags, room->sector);
    if (room->coord_assigned) {
        fprintf(fp, "    Coord: (%d, %d, %d)\n", room->coord.x, room->coord.y, room->coord.z);
    }
    fprintf(fp, "    Exits:");
    bool has_exits = false;
    for (int d = 0; d < DIKU_MAX_EXITS; d++) {
        if (room->exits[d] && room->exits[d]->to_vnum > 0) {
            fprintf(fp, " %s->#%d", diku_dir_name_short(d), room->exits[d]->to_vnum);
            has_exits = true;
        }
    }
    if (!has_exits) fprintf(fp, " (none)");
    fprintf(fp, "\n");
    if (room->extra_desc_count > 0) {
        fprintf(fp, "    Extra descs: %d\n", room->extra_desc_count);
    }
}

void diku_print_mobile_fp(FILE *fp, const mobile_t *mob) {
    if (!mob) {
        fprintf(fp, "  (null mobile)\n");
        return;
    }
    fprintf(fp, "  Mobile #%d: %s\n", mob->vnum,
            mob->short_desc.str ? mob->short_desc.str : "(unnamed)");
    fprintf(fp, "    Name: %s\n", mob->name.str ? mob->name.str : "(none)");
    fprintf(fp, "    Long: %s\n", mob->long_desc.str ? mob->long_desc.str : "(none)");
    fprintf(fp, "    Desc: %.60s%s\n",
            mob->description.str ? mob->description.str : "(none)",
            mob->description.len > 60 ? "..." : "");
    fprintf(fp, "    Level: %d, Align: %d, Sex: %d, Race: %d\n",
            mob->level, mob->alignment, mob->sex, mob->race);
    fprintf(fp, "    Hitroll: %d, Damroll: %d\n", mob->hitroll, mob->damroll);
    fprintf(fp, "    AC: %d/%d/%d/%d\n", mob->ac[0], mob->ac[1], mob->ac[2], mob->ac[3]);
    fprintf(fp, "    Gold: %ld, Silver: %ld\n", mob->gold, mob->silver);
    fprintf(fp, "    Size: %d, Form: %d, Parts: %d\n", mob->size, mob->form, mob->parts);
    if (mob->material.len > 0) {
        fprintf(fp, "    Material: %s\n", mob->material.str);
    }
}

void diku_print_item_fp(FILE *fp, const item_t *item) {
    if (!item) {
        fprintf(fp, "  (null item)\n");
        return;
    }
    fprintf(fp, "  Object #%d: %s\n", item->vnum,
            item->short_desc.str ? item->short_desc.str : "(unnamed)");
    fprintf(fp, "    Name: %s\n", item->name.str ? item->name.str : "(none)");
    fprintf(fp, "    Long: %s\n", item->long_desc.str ? item->long_desc.str : "(none)");
    fprintf(fp, "    Desc: %.60s%s\n",
            item->description.str ? item->description.str : "(none)",
            item->description.len > 60 ? "..." : "");
    fprintf(fp, "    Type: %s, Weight: %d, Cost: %d, Level: %d\n",
            diku_item_type_name(item->type), item->weight, item->cost, item->level);
    fprintf(fp, "    Values: %d %d %d %d %d\n",
            item->value[0], item->value[1], item->value[2], item->value[3], item->value[4]);
    fprintf(fp, "    Extra flags: 0x%x, Wear flags: 0x%x\n",
            item->extra_flags, item->wear_flags);
    if (item->material.len > 0) {
        fprintf(fp, "    Material: %s\n", item->material.str);
    }
    if (item->affect_count > 0) {
        fprintf(fp, "    Affects: %d\n", item->affect_count);
    }
    if (item->extra_desc_count > 0) {
        fprintf(fp, "    Extra descs: %d\n", item->extra_desc_count);
    }
}

void diku_print_area_info_fp(FILE *fp, const area_t *area) {
    if (!area) {
        fprintf(fp, "(null area)\n");
        return;
    }
    fprintf(fp, "Area: %s\n", area->name.str ? area->name.str : "(unnamed)");
    fprintf(fp, "  File: %s\n", area->filename.str ? area->filename.str : "(unknown)");
    fprintf(fp, "  Builders: %s\n", area->builders.str ? area->builders.str : "(none)");
    fprintf(fp, "  Rooms: %d\n", area->room_count);
    fprintf(fp, "  Mobiles: %d\n", area->mobile_count);
    fprintf(fp, "  Items: %d\n", area->item_count);
    if (area->low_vnum > 0 || area->high_vnum > 0) {
        fprintf(fp, "  Vnum range: %d - %d\n", area->low_vnum, area->high_vnum);
    }
}

void diku_print_graph_fp(FILE *fp, const area_t *area) {
    if (!area) return;
    fprintf(fp, "\nGraph for area: %s\n", area->name.str ? area->name.str : "(unnamed)");
    fprintf(fp, "================\n");
    for (int i = 0; i < area->room_count; i++) {
        diku_print_room_fp(fp, &area->rooms[i]);
    }
}

void diku_print_coordinates_fp(FILE *fp, area_t *area) {
    if (!area) return;
    fprintf(fp, "\n3D Coordinates for area: %s\n", area->name.str ? area->name.str : "(unnamed)");
    fprintf(fp, "========================\n");
    int min_x, max_x, min_y, max_y, min_z, max_z;
    diku_get_coord_bounds(area, &min_x, &max_x, &min_y, &max_y, &min_z, &max_z);
    fprintf(fp, "Bounds: X[%d,%d] Y[%d,%d] Z[%d,%d]\n",
            min_x, max_x, min_y, max_y, min_z, max_z);
    for (int z = max_z; z >= min_z; z--) {
        bool has_rooms = false;
        for (int i = 0; i < area->room_count; i++) {
            if (area->rooms[i].coord_assigned && area->rooms[i].coord.z == z) {
                has_rooms = true;
                break;
            }
        }
        if (!has_rooms) continue;
        fprintf(fp, "\n--- Z = %d ---\n", z);
        for (int y = max_y; y >= min_y; y--) {
            for (int x = min_x; x <= max_x; x++) {
                room_t *room = diku_room_at_coord(area, x, y, z);
                if (room) {
                    fprintf(fp, "[%4d]", room->vnum % 10000);
                } else {
                    fprintf(fp, "  ..  ");
                }
            }
            fprintf(fp, "\n");
        }
    }
}

#endif /* DIKU_PARSER_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif
#endif /* DIKU_DEBUG_H */
