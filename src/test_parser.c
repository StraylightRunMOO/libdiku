#include "diku_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef enum {
    PAGE_NEXT,
    PAGE_QUIT,
    PAGE_SKIP_CATEGORY
} page_action_t;

static page_action_t pager_prompt(const char *category_name, int current, int total)
{
    printf("  [%s %d/%d]  [Enter: next | q: quit | n: next category] ",
           category_name, current, total);
    fflush(stdout);

    char buf[16];
    if (!fgets(buf, sizeof(buf), stdin)) {
        return PAGE_QUIT;
    }

    if (buf[0] == 'q' || buf[0] == 'Q') return PAGE_QUIT;
    if (buf[0] == 'n' || buf[0] == 'N') return PAGE_SKIP_CATEGORY;
    return PAGE_NEXT;
}

static void print_usage(const char *prog)
{
    printf("Usage: %s <area_file.are> [options]\n", prog);
    printf("\nOptions:\n");
    printf("  -v, --verbose   Verbose mode: page through selected entities\n");
    printf("  --all           Select all entities (default with -v)\n");
    printf("  --rooms         Select rooms\n");
    printf("  --mobiles       Select mobiles\n");
    printf("  --objects       Select objects/items\n");
    printf("  --graph         Select graph connections\n");
    printf("  --coords        Select 3D coordinates\n");
    printf("  --symmetry      Show exit symmetry report\n");
    printf("  -h, --help      Show this help\n");
    printf("\nWithout -v, an overview summary is printed.\n");
}

static void print_overview(area_t *area)
{
    diku_format_t fmt = diku_detect_format(area);
    room_t *central = diku_find_central_room(area);

    int total_exits = 0;
    int resolved_exits = 0;
    for (int i = 0; i < area->room_count; i++) {
        for (int d = 0; d < DIKU_MAX_EXITS; d++) {
            if (area->rooms[i].exits[d] && area->rooms[i].exits[d]->to_vnum > 0) {
                total_exits++;
                if (area->rooms[i].exits[d]->to_room) {
                    resolved_exits++;
                }
            }
        }
    }

    int min_x, max_x, min_y, max_y, min_z, max_z;
    diku_get_coord_bounds(area, &min_x, &max_x, &min_y, &max_y, &min_z, &max_z);
    int diameter = diku_graph_diameter(area);
    int sym_total = 0, sym_asymmetric = 0;
    diku_check_exit_symmetry(area, &sym_total, &sym_asymmetric);

    printf("============================================================\n");
    printf("Area Overview: %s\n", area->filename.str ? area->filename.str : "(unknown)");
    printf("============================================================\n");
    printf("Format:     %s\n", diku_format_name(fmt));
    printf("Name:       %s\n", area->name.str ? area->name.str : "(unnamed)");
    printf("Builders:   %s\n", area->builders.str ? area->builders.str : "(none)");
    if (area->credits.str && area->credits.len > 0) {
        printf("Credits:    %s\n", area->credits.str);
    }
    if (area->low_level > 0 || area->high_level > 0) {
        printf("Levels:     %d - %d\n", area->low_level, area->high_level);
    }
    if (area->low_vnum > 0 || area->high_vnum > 0) {
        printf("Vnums:      %d - %d\n", area->low_vnum, area->high_vnum);
    }
    if (area->security > 0) {
        printf("Security:   %d\n", area->security);
    }
    if (area->version > 0) {
        printf("Version:    %d\n", area->version);
    }
    printf("\n");
    printf("Entities:\n");
    printf("  Rooms:    %d\n", area->room_count);
    printf("  Mobiles:  %d\n", area->mobile_count);
    printf("  Objects:  %d\n", area->item_count);
    printf("  Exits:    %d (%.1f%% resolved)\n",
           total_exits, total_exits > 0 ? (100.0 * resolved_exits / total_exits) : 0.0);
    printf("\n");
    printf("Coordinates:\n");
    printf("  Bounds:   X[%d,%d] Y[%d,%d] Z[%d,%d]\n",
           min_x, max_x, min_y, max_y, min_z, max_z);
    printf("  Central:  #%d - %s\n",
           central ? central->vnum : -1,
           central ? (central->name.str ? central->name.str : "(unnamed)") : "(none)");
    printf("  Diameter: %d\n", diameter);
    printf("  Symmetry: %d/%d asymmetric exits\n", sym_asymmetric, sym_total);
    printf("============================================================\n");
}

static bool page_rooms(area_t *area)
{
    if (area->room_count == 0) {
        printf("\n--- Rooms (0) ---\n");
        return false;
    }
    printf("\n--- Rooms (%d) ---\n", area->room_count);
    for (int i = 0; i < area->room_count; i++) {
        printf("\n");
        diku_print_room(&area->rooms[i]);
        page_action_t action = pager_prompt("Room", i + 1, area->room_count);
        if (action == PAGE_QUIT) return true;
        if (action == PAGE_SKIP_CATEGORY) return false;
    }
    return false;
}

static bool page_mobiles(area_t *area)
{
    if (area->mobile_count == 0) {
        printf("\n--- Mobiles (0) ---\n");
        return false;
    }
    printf("\n--- Mobiles (%d) ---\n", area->mobile_count);
    for (int i = 0; i < area->mobile_count; i++) {
        printf("\n");
        diku_print_mobile(&area->mobiles[i]);
        page_action_t action = pager_prompt("Mobile", i + 1, area->mobile_count);
        if (action == PAGE_QUIT) return true;
        if (action == PAGE_SKIP_CATEGORY) return false;
    }
    return false;
}

static bool page_objects(area_t *area)
{
    if (area->item_count == 0) {
        printf("\n--- Objects (0) ---\n");
        return false;
    }
    printf("\n--- Objects (%d) ---\n", area->item_count);
    for (int i = 0; i < area->item_count; i++) {
        printf("\n");
        diku_print_item(&area->items[i]);
        page_action_t action = pager_prompt("Object", i + 1, area->item_count);
        if (action == PAGE_QUIT) return true;
        if (action == PAGE_SKIP_CATEGORY) return false;
    }
    return false;
}

static bool page_graph(area_t *area)
{
    if (area->room_count == 0) {
        printf("\n--- Graph (0 rooms) ---\n");
        return false;
    }
    printf("\n--- Graph Connections (%d rooms) ---\n", area->room_count);
    for (int i = 0; i < area->room_count; i++) {
        printf("\n");
        diku_print_room(&area->rooms[i]);
        page_action_t action = pager_prompt("Graph", i + 1, area->room_count);
        if (action == PAGE_QUIT) return true;
        if (action == PAGE_SKIP_CATEGORY) return false;
    }
    return false;
}

static bool page_coords(area_t *area)
{
    printf("\n--- 3D Coordinates ---\n\n");
    diku_print_coordinates(area);
    printf("\n");
    page_action_t action = pager_prompt("Coords", 1, 1);
    if (action == PAGE_QUIT) return true;
    return false;
}

static bool page_symmetry(area_t *area)
{
    int total = 0;
    int asymmetric = 0;
    diku_check_exit_symmetry(area, &total, &asymmetric);
    
    if (asymmetric == 0) {
        printf("\n--- Exit Symmetry ---\n");
        printf("All exits are symmetric (%d exits checked).\n", total);
        page_action_t action = pager_prompt("Symmetry", 1, 1);
        if (action == PAGE_QUIT) return true;
        return false;
    }
    
    printf("\n--- Exit Symmetry (%d of %d asymmetric) ---\n", asymmetric, total);
    
    int current = 0;
    for (int i = 0; i < area->room_count; i++) {
        room_t *room = &area->rooms[i];
        for (int d = 0; d < DIKU_MAX_EXITS; d++) {
            exit_t *exit = room->exits[d];
            if (!exit || !exit->to_room) continue;
            
            int rev = diku_reverse_dir(d);
            room_t *target = exit->to_room;
            exit_t *return_exit = NULL;
            if (rev >= 0 && rev < DIKU_MAX_EXITS) {
                return_exit = target->exits[rev];
            }
            
            if (!return_exit || return_exit->to_room != room) {
                current++;
                printf("\n");
                printf("  #%d -> %s -> #%d missing return %s",
                       room->vnum, diku_dir_name(d), target->vnum, diku_dir_name(rev));
                if (return_exit && return_exit->to_room) {
                    printf(" (goes to #%d instead)\n", return_exit->to_room->vnum);
                } else if (return_exit) {
                    printf(" (unresolved)\n");
                } else {
                    printf("\n");
                }
                page_action_t action = pager_prompt("Symmetry", current, asymmetric);
                if (action == PAGE_QUIT) return true;
                if (action == PAGE_SKIP_CATEGORY) return false;
            }
        }
    }
    return false;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *filename = NULL;
    bool verbose = false;
    bool show_rooms = false;
    bool show_mobiles = false;
    bool show_objects = false;
    bool show_graph = false;
    bool show_coords = false;
    bool show_symmetry = false;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
                verbose = true;
            } else if (strcmp(argv[i], "--all") == 0) {
                show_rooms = show_mobiles = show_objects = show_graph = show_coords = true;
            } else if (strcmp(argv[i], "--rooms") == 0) {
                show_rooms = true;
            } else if (strcmp(argv[i], "--mobiles") == 0) {
                show_mobiles = true;
            } else if (strcmp(argv[i], "--objects") == 0) {
                show_objects = true;
            } else if (strcmp(argv[i], "--graph") == 0) {
                show_graph = true;
            } else if (strcmp(argv[i], "--coords") == 0) {
                show_coords = true;
            } else if (strcmp(argv[i], "--symmetry") == 0) {
                show_symmetry = true;
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_usage(argv[0]);
                return 0;
            }
        } else {
            filename = argv[i];
        }
    }

    if (!filename) {
        printf("Error: No area file specified\n");
        print_usage(argv[0]);
        return 1;
    }

    area_t *area = diku_parse_file(filename);
    if (!area) {
        printf("Error: Failed to parse %s (errno=%d: %s)\n",
               filename, errno, strerror(errno));
        return 1;
    }

    diku_resolve_graph_global(area);
    room_t *central = diku_find_central_room(area);
    if (central) {
        diku_assign_coordinates(area, central);
    }

    if (verbose) {
        /* If no specific entity type selected, default to --all */
        if (!show_rooms && !show_mobiles && !show_objects && !show_graph && !show_coords && !show_symmetry) {
            show_rooms = show_mobiles = show_objects = show_graph = show_coords = show_symmetry = true;
        }

        bool quit = false;
        if (!quit && show_rooms)  quit = page_rooms(area);
        if (!quit && show_mobiles) quit = page_mobiles(area);
        if (!quit && show_objects) quit = page_objects(area);
        if (!quit && show_graph)  quit = page_graph(area);
        if (!quit && show_coords) quit = page_coords(area);
        if (!quit && show_symmetry) quit = page_symmetry(area);
    } else {
        print_overview(area);
        if (show_symmetry) {
            printf("\n");
            diku_print_exit_symmetry(area);
        }
    }

    diku_free_area(area);
    return 0;
}
