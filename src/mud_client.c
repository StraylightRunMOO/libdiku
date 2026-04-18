#include "diku.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

static area_t *g_areas = NULL;
static area_t *g_start_area = NULL;
static room_t *g_room = NULL;

static void print_separator(void)
{
    printf("\n");
}

static const char *article_for(const char *str)
{
    if (!str || !str[0]) return "a";
    char c = tolower((unsigned char)str[0]);
    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') return "an";
    return "a";
}

static char *lowercase(const char *str)
{
    static char buf[256];
    size_t i;
    for (i = 0; i < sizeof(buf) - 1 && str[i]; i++)
        buf[i] = tolower((unsigned char)str[i]);
    buf[i] = '\0';
    return buf;
}

static void print_exits(void)
{
    bool has_any = false;
    for (int d = 0; d < DIKU_MAX_EXITS; d++) {
        if (g_room->exits[d] && g_room->exits[d]->to_vnum > 0) {
            has_any = true;
            break;
        }
    }

    if (!has_any) {
        printf("There are no obvious exits.\n");
        return;
    }

    printf("There are exits to the ");
    bool first = true;
    for (int d = 0; d < DIKU_MAX_EXITS; d++) {
        if (g_room->exits[d] && g_room->exits[d]->to_vnum > 0) {
            if (!first) {
                bool last = true;
                for (int dd = d + 1; dd < DIKU_MAX_EXITS; dd++) {
                    if (g_room->exits[dd] && g_room->exits[dd]->to_vnum > 0) {
                        last = false;
                        break;
                    }
                }
                printf(last ? " and " : ", ");
            }
            printf("%s(%s)", diku_dir_name(d), diku_dir_name_short(d));
            first = false;
        }
    }
    printf(".\n");
}

static void print_mobs(void)
{
    if (g_room->room_mobile_count == 0) return;

    for (int i = 0; i < g_room->room_mobile_count; i++) {
        mobile_t *mob = g_room->room_mobiles[i];
        if (mob->long_desc.str && mob->long_desc.len > 0) {
            printf("%s\n", mob->long_desc.str);
        } else if (mob->short_desc.str && mob->short_desc.len > 0) {
            printf("%s is here.\n", mob->short_desc.str);
        } else {
            printf("A creature is here.\n");
        }
    }
}

static void print_items(void)
{
    if (g_room->room_item_count == 0) return;

    typedef struct {
        const char *name;
        int count;
    } item_group_t;

    item_group_t groups[64];
    int group_count = 0;

    for (int i = 0; i < g_room->room_item_count; i++) {
        const char *name = g_room->room_items[i]->short_desc.str;
        if (!name) name = "something";
        int j;
        for (j = 0; j < group_count; j++) {
            if (strcmp(groups[j].name, name) == 0) {
                groups[j].count++;
                break;
            }
        }
        if (j == group_count && group_count < 64) {
            groups[group_count].name = name;
            groups[group_count].count = 1;
            group_count++;
        }
    }

    if (group_count == 1) {
        if (groups[0].count == 1) {
            printf("You see %s here.\n", groups[0].name);
        } else {
            printf("You see %d %s here.\n", groups[0].count, groups[0].name);
        }
        return;
    }

    printf("You see ");
    for (int i = 0; i < group_count; i++) {
        if (i > 0) {
            if (i == group_count - 1)
                printf(" and ");
            else
                printf(", ");
        }
        if (groups[i].count == 1) {
            printf("%s", groups[i].name);
        } else {
            printf("%d %s", groups[i].count, groups[i].name);
        }
    }
    printf(".\n");
}

static void look_room(void)
{
    if (!g_room) {
        printf("You are nowhere.\n");
        return;
    }

    printf("%s (#%d)", g_room->name.str ? g_room->name.str : "(unnamed)", g_room->vnum);
    if (g_room->coord_assigned) {
        printf(" (%d, %d, %d)", g_room->coord.x, g_room->coord.y, g_room->coord.z);
    }
    printf("\n");

    if (g_room->desc.str && g_room->desc.len > 0) {
        printf("%s\n", g_room->desc.str);
    }

    print_separator();
    print_mobs();
    print_items();
    print_separator();
    print_exits();
}

static void look_target(const char *target)
{
    if (!target || !target[0]) {
        look_room();
        return;
    }

    mobile_t *mob = diku_find_mobile_in_room_by_name(g_room, target);
    if (mob) {
        if (mob->description.str && mob->description.len > 0) {
            printf("%s\n", mob->description.str);
        } else if (mob->long_desc.str && mob->long_desc.len > 0) {
            printf("%s\n", mob->long_desc.str);
        } else {
            printf("You see %s.\n", mob->short_desc.str ? mob->short_desc.str : "a creature");
        }
        if (mob->inventory_count > 0) {
            printf("\nThey are carrying:\n");
            for (int j = 0; j < mob->inventory_count; j++) {
                item_t *item = mob->inventory[j].item;
                if (mob->inventory[j].wear_loc >= 0) {
                    printf("  %s (worn)\n", item->short_desc.str ? item->short_desc.str : "something");
                } else {
                    printf("  %s\n", item->short_desc.str ? item->short_desc.str : "something");
                }
            }
        }
        return;
    }

    item_t *item = diku_find_item_in_room_by_name(g_room, target);
    if (item) {
        if (item->description.str && item->description.len > 0) {
            printf("%s\n", item->description.str);
        } else {
            printf("You see %s.\n", item->short_desc.str ? item->short_desc.str : "something");
        }
        return;
    }

    for (int i = 0; i < g_room->extra_desc_count; i++) {
        if (diku_name_matches(g_room->extra_descs[i].keywords.str, target)) {
            printf("%s\n", g_room->extra_descs[i].desc.str);
            return;
        }
    }

    printf("You do not see that here.\n");
}

static int dir_from_cmd(const char *cmd)
{
    if (strcmp(cmd, "n") == 0 || strcmp(cmd, "north") == 0) return DIR_NORTH;
    if (strcmp(cmd, "e") == 0 || strcmp(cmd, "east") == 0) return DIR_EAST;
    if (strcmp(cmd, "s") == 0 || strcmp(cmd, "south") == 0) return DIR_SOUTH;
    if (strcmp(cmd, "w") == 0 || strcmp(cmd, "west") == 0) return DIR_WEST;
    if (strcmp(cmd, "u") == 0 || strcmp(cmd, "up") == 0) return DIR_UP;
    if (strcmp(cmd, "d") == 0 || strcmp(cmd, "down") == 0) return DIR_DOWN;
    if (strcmp(cmd, "ne") == 0 || strcmp(cmd, "northeast") == 0) return DIR_NORTHEAST;
    if (strcmp(cmd, "nw") == 0 || strcmp(cmd, "northwest") == 0) return DIR_NORTHWEST;
    if (strcmp(cmd, "se") == 0 || strcmp(cmd, "southeast") == 0) return DIR_SOUTHEAST;
    if (strcmp(cmd, "sw") == 0 || strcmp(cmd, "southwest") == 0) return DIR_SOUTHWEST;
    if (strcmp(cmd, "in") == 0) return DIR_IN;
    if (strcmp(cmd, "out") == 0) return DIR_OUT;
    return -1;
}

static void move_dir(int dir)
{
    if (!g_room) return;
    if (dir < 0 || dir >= DIKU_MAX_EXITS || !g_room->exits[dir] || !g_room->exits[dir]->to_room) {
        printf("Alas, you cannot go that way.\n");
        return;
    }
    g_room = g_room->exits[dir]->to_room;
    look_room();
}

static room_t *find_room_containing_vnum(int vnum)
{
    for (area_t *a = g_areas; a; a = a->next) {
        for (int i = 0; i < a->room_count; i++) {
            room_t *r = &a->rooms[i];
            for (int j = 0; j < r->room_item_count; j++) {
                if (r->room_items[j]->vnum == vnum) return r;
            }
            for (int j = 0; j < r->room_mobile_count; j++) {
                mobile_t *mob = r->room_mobiles[j];
                if (mob->vnum == vnum) return r;
                for (int k = 0; k < mob->inventory_count; k++) {
                    if (mob->inventory[k].item->vnum == vnum) return r;
                }
            }
        }
    }
    return NULL;
}

static void cmd_go(const char *arg)
{
    while (*arg && isspace((unsigned char)*arg)) arg++;
    if (!arg[0]) {
        printf("Usage: @go #<vnum>\n");
        return;
    }

    if (arg[0] == '#') arg++;
    char *endptr;
    long vnum_l = strtol(arg, &endptr, 10);
    if (*endptr != '\0' && !isspace((unsigned char)*endptr)) {
        printf("Usage: @go #<vnum>\n");
        return;
    }
    int vnum = (int)vnum_l;

    room_t *target = diku_find_room_global(g_areas, vnum);
    if (!target) {
        target = find_room_containing_vnum(vnum);
    }

    if (!target) {
        printf("No such room or entity.\n");
        return;
    }

    g_room = target;
    printf("You are transported to...\n");
    print_separator();
    look_room();
}

static void cmd_zap(void)
{
    int total = 0;
    for (area_t *a = g_areas; a; a = a->next) {
        total += a->room_count;
    }
    if (total == 0) {
        printf("No rooms to zap to!\n");
        return;
    }

    int idx = rand() % total;
    room_t *target = NULL;
    for (area_t *a = g_areas; a; a = a->next) {
        if (idx < a->room_count) {
            target = &a->rooms[idx];
            break;
        }
        idx -= a->room_count;
    }

    if (target) {
        g_room = target;
        printf("*ZAP* You have been transported to a random location!\n");
        print_separator();
        look_room();
    }
}

static void print_help(void)
{
    printf("Commands:\n");
    printf("  n, s, e, w, ne, nw, se, sw, u, d, in, out  Move in a direction\n");
    printf("  look [target]    Look at the room or something in it (alias: l)\n");
    printf("  @go #<vnum>      Teleport to a room (or room containing vnum)\n");
    printf("  zap              Teleport to a random room\n");
    printf("  help             Show this help\n");
    printf("  quit, q          Exit\n");
}

static void process_cmd(char *cmd)
{
    size_t len = strlen(cmd);
    while (len > 0 && (cmd[len - 1] == '\n' || cmd[len - 1] == '\r' || isspace((unsigned char)cmd[len - 1])))
        cmd[--len] = '\0';

    while (*cmd && isspace((unsigned char)*cmd))
        cmd++;

    if (!cmd[0]) return;

    int dir = dir_from_cmd(cmd);
    if (dir >= 0) {
        move_dir(dir);
        return;
    }

    if (strncmp(cmd, "look ", 5) == 0 || strncmp(cmd, "l ", 2) == 0) {
        const char *target = cmd;
        if (target[0] == 'l' && target[1] == ' ') target += 2;
        else if (target[0] == 'l' && target[1] == 'o' && target[2] == 'o' && target[3] == 'k' && target[4] == ' ') target += 5;
        while (*target && isspace((unsigned char)*target)) target++;
        look_target(target);
        return;
    }
    if (strcmp(cmd, "look") == 0 || strcmp(cmd, "l") == 0) {
        look_room();
        return;
    }

    if (strcmp(cmd, "zap") == 0) {
        cmd_zap();
        return;
    }

    if (strncmp(cmd, "@go", 3) == 0) {
        cmd_go(cmd + 3);
        return;
    }

    if (strcmp(cmd, "help") == 0) {
        print_help();
        return;
    }

    if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "q") == 0) {
        printf("Goodbye!\n");
        exit(0);
    }

    printf("Huh?\n");
}

int main(int argc, char *argv[])
{
    const char *path = (argc > 1) ? argv[1] : "data";

    diku_context_t *ctx = diku_context_create();
    if (!ctx) {
        printf("Error: Failed to create context\n");
        return 1;
    }

    g_areas = diku_parse_path(ctx, path);
    if (!g_areas) {
        printf("Error: Failed to load any areas from %s\n", path);
        diku_context_destroy(ctx);
        return 1;
    }

    diku_resolve_graph_global(ctx, g_areas);

    g_start_area = g_areas;
    g_room = diku_find_central_room(g_start_area);
    if (!g_room && g_start_area->room_count > 0) {
        g_room = &g_start_area->rooms[0];
    }

    if (g_room) {
        diku_assign_coordinates_multi(ctx, g_areas);
    }

    int area_count = 0;
    int total_rooms = 0;
    for (area_t *a = g_areas; a; a = a->next) {
        area_count++;
        total_rooms += a->room_count;
    }

    srand((unsigned)time(NULL));

    printf("Welcome to the DikuMUD Area Explorer!\n");
    printf("Loaded %d area(s) with %d total rooms.\n", area_count, total_rooms);
    printf("Type 'help' for commands, 'quit' to exit.\n");
    print_separator();
    look_room();

    char buf[256];
    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) {
            break;
        }
        process_cmd(buf);
    }

    diku_free_all_areas(g_areas);
    diku_context_destroy(ctx);
    return 0;
}
