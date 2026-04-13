#include "diku_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <glob.h>
#include <sys/stat.h>

static area_t *g_areas = NULL;
static area_t *g_start_area = NULL;
static room_t *g_room = NULL;

static area_t *load_areas(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return NULL;
    
    area_t *head = NULL;
    area_t *tail = NULL;
    
    if (S_ISREG(st.st_mode)) {
        area_t *area = diku_parse_file(path);
        if (area) {
            area->next = NULL;
            head = tail = area;
        }
    } else if (S_ISDIR(st.st_mode)) {
        char pattern[1024];
        int n = snprintf(pattern, sizeof(pattern), "%s/*.are", path);
        if (n > 0 && n < (int)sizeof(pattern)) {
            glob_t globbuf;
            if (glob(pattern, 0, NULL, &globbuf) == 0) {
                for (size_t i = 0; i < globbuf.gl_pathc; i++) {
                    area_t *area = diku_parse_file(globbuf.gl_pathv[i]);
                    if (area) {
                        area->next = NULL;
                        if (!head) head = area;
                        if (tail) tail->next = area;
                        tail = area;
                    }
                }
                globfree(&globbuf);
            }
        }
    }
    
    return head;
}

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

static bool name_matches(const char *name, const char *keyword)
{
    if (!name || !keyword) return false;
    char nbuf[256], kbuf[256];
    strncpy(nbuf, name, sizeof(nbuf) - 1);
    nbuf[sizeof(nbuf) - 1] = '\0';
    strncpy(kbuf, keyword, sizeof(kbuf) - 1);
    kbuf[sizeof(kbuf) - 1] = '\0';
    
    char *ntok = strtok(nbuf, " ");
    while (ntok) {
        if (strstr(lowercase(ntok), lowercase(kbuf)) != NULL)
            return true;
        ntok = strtok(NULL, " ");
    }
    return false;
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
                /* Check if this is the last one */
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
    
    /* Count by short_desc */
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
    
    /* Search mobs */
    for (int i = 0; i < g_room->room_mobile_count; i++) {
        mobile_t *mob = g_room->room_mobiles[i];
        if (name_matches(mob->name.str, target) ||
            name_matches(mob->short_desc.str, target)) {
            if (mob->description.str && mob->description.len > 0) {
                printf("%s\n", mob->description.str);
            } else if (mob->long_desc.str && mob->long_desc.len > 0) {
                printf("%s\n", mob->long_desc.str);
            } else {
                printf("You see %s.\n", mob->short_desc.str ? mob->short_desc.str : "a creature");
            }
            
            /* Show inventory */
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
    }
    
    /* Search items */
    for (int i = 0; i < g_room->room_item_count; i++) {
        item_t *item = g_room->room_items[i];
        if (name_matches(item->name.str, target) ||
            name_matches(item->short_desc.str, target)) {
            if (item->description.str && item->description.len > 0) {
                printf("%s\n", item->description.str);
            } else {
                printf("You see %s.\n", item->short_desc.str ? item->short_desc.str : "something");
            }
            return;
        }
    }
    
    /* Search extra descs */
    for (int i = 0; i < g_room->extra_desc_count; i++) {
        if (name_matches(g_room->extra_descs[i].keywords.str, target)) {
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

static void print_help(void)
{
    printf("Commands:\n");
    printf("  n, s, e, w, ne, nw, se, sw, u, d, in, out  Move in a direction\n");
    printf("  look [target]    Look at the room or something in it (alias: l)\n");
    printf("  help             Show this help\n");
    printf("  quit, q          Exit\n");
}

static void process_cmd(char *cmd)
{
    /* Trim newline and trailing spaces */
    size_t len = strlen(cmd);
    while (len > 0 && (cmd[len - 1] == '\n' || cmd[len - 1] == '\r' || isspace((unsigned char)cmd[len - 1])))
        cmd[--len] = '\0';
    
    /* Skip leading spaces */
    while (*cmd && isspace((unsigned char)*cmd))
        cmd++;
    
    if (!cmd[0]) return;
    
    /* Check for movement */
    int dir = dir_from_cmd(cmd);
    if (dir >= 0) {
        move_dir(dir);
        return;
    }
    
    /* Look command */
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
    
    g_areas = load_areas(path);
    if (!g_areas) {
        printf("Error: Failed to load any areas from %s\n", path);
        return 1;
    }
    
    diku_resolve_graph_global(g_areas);
    
    g_start_area = g_areas;
    g_room = diku_find_central_room(g_start_area);
    if (!g_room && g_start_area->room_count > 0) {
        g_room = &g_start_area->rooms[0];
    }
    
    if (g_room) {
        diku_assign_coordinates_multi(g_areas);
    }
    
    /* Count loaded areas and rooms for user feedback */
    int area_count = 0;
    int total_rooms = 0;
    for (area_t *a = g_areas; a; a = a->next) {
        area_count++;
        total_rooms += a->room_count;
    }
    
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
    return 0;
}
