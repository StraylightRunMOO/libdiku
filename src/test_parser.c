#include "diku_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

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
    printf("Usage: %s <path> [options]\n", prog);
    printf("\nPath can be:\n");
    printf("  <area_file.are>     A single area file\n");
    printf("  <package_base>      Base path to a classic .wld/.mob/.obj/.zon package\n");
    printf("  <directory>         Load all .are files (or packages with --packages)\n");
    printf("\nOptions:\n");
    printf("  -v, --verbose       Verbose mode: page through selected entities\n");
    printf("  --all               Select all entities (default with -v)\n");
    printf("  --rooms             Select rooms\n");
    printf("  --mobiles           Select mobiles\n");
    printf("  --objects           Select objects/items\n");
    printf("  --graph             Select graph connections\n");
    printf("  --coords            Select 3D coordinates\n");
    printf("  --symmetry          Show exit symmetry report\n");
    printf("  --packages          When loading a directory, load multi-file packages\n");
    printf("  -h, --help          Show this help\n");
    printf("\nWithout -v, an overview summary is printed.\n");
}

static bool is_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

static void progress_cb(const char *op, int cur, int total, const char *detail, void *user)
{
    (void)user;
    if (total > 0) {
        fprintf(stderr, "\r[%s] %d/%d %s", op, cur + 1, total, detail ? detail : "");
        if (cur + 1 >= total) fprintf(stderr, "\n");
        fflush(stderr);
    } else {
        fprintf(stderr, "[%s] %s\n", op, detail ? detail : "");
    }
}

static void print_overview(area_t *areas, const char *source)
{
    int total_areas = 0, total_rooms = 0, total_mobiles = 0, total_items = 0;
    int total_exits = 0, total_resolved = 0;
    int sym_total = 0, sym_asymmetric = 0;

    for (area_t *a = areas; a; a = a->next) {
        total_areas++;
        total_rooms += a->room_count;
        total_mobiles += a->mobile_count;
        total_items += a->item_count;

        for (int i = 0; i < a->room_count; i++) {
            for (int d = 0; d < DIKU_MAX_EXITS; d++) {
                exit_t *e = a->rooms[i].exits[d];
                if (e && e->to_vnum > 0) {
                    total_exits++;
                    if (e->to_room) total_resolved++;
                }
            }
        }

        int t = 0, asym = 0;
        diku_check_exit_symmetry(a, &t, &asym);
        sym_total += t;
        sym_asymmetric += asym;
    }

    printf("============================================================\n");
    printf("Source: %s\n", source);
    printf("Areas loaded: %d\n", total_areas);
    printf("============================================================\n");
    printf("Total Rooms:    %d\n", total_rooms);
    printf("Total Mobiles:  %d\n", total_mobiles);
    printf("Total Objects:  %d\n", total_items);
    printf("Total Exits:    %d (%.1f%% resolved)\n",
           total_exits, total_exits > 0 ? (100.0 * total_resolved / total_exits) : 0.0);
    printf("Symmetry:       %d/%d asymmetric exits\n", sym_asymmetric, sym_total);
    printf("============================================================\n");

    for (area_t *a = areas; a; a = a->next) {
        printf("  %-30s  R:%4d  M:%4d  O:%4d\n",
               a->name.str && a->name.len > 0 ? a->name.str : "(unnamed)",
               a->room_count, a->mobile_count, a->item_count);
    }
    printf("============================================================\n");
}

static bool page_rooms(area_t *areas)
{
    int total = 0;
    for (area_t *a = areas; a; a = a->next) total += a->room_count;

    if (total == 0) {
        printf("\n--- Rooms (0) ---\n");
        return false;
    }
    printf("\n--- Rooms (%d) ---\n", total);

    int idx = 0;
    for (area_t *a = areas; a; a = a->next) {
        for (int i = 0; i < a->room_count; i++) {
            printf("\n");
            diku_print_room(&a->rooms[i]);
            page_action_t action = pager_prompt("Room", ++idx, total);
            if (action == PAGE_QUIT) return true;
            if (action == PAGE_SKIP_CATEGORY) return false;
        }
    }
    return false;
}

static bool page_mobiles(area_t *areas)
{
    int total = 0;
    for (area_t *a = areas; a; a = a->next) total += a->mobile_count;

    if (total == 0) {
        printf("\n--- Mobiles (0) ---\n");
        return false;
    }
    printf("\n--- Mobiles (%d) ---\n", total);

    int idx = 0;
    for (area_t *a = areas; a; a = a->next) {
        for (int i = 0; i < a->mobile_count; i++) {
            printf("\n");
            diku_print_mobile(&a->mobiles[i]);
            page_action_t action = pager_prompt("Mobile", ++idx, total);
            if (action == PAGE_QUIT) return true;
            if (action == PAGE_SKIP_CATEGORY) return false;
        }
    }
    return false;
}

static bool page_objects(area_t *areas)
{
    int total = 0;
    for (area_t *a = areas; a; a = a->next) total += a->item_count;

    if (total == 0) {
        printf("\n--- Objects (0) ---\n");
        return false;
    }
    printf("\n--- Objects (%d) ---\n", total);

    int idx = 0;
    for (area_t *a = areas; a; a = a->next) {
        for (int i = 0; i < a->item_count; i++) {
            printf("\n");
            diku_print_item(&a->items[i]);
            page_action_t action = pager_prompt("Object", ++idx, total);
            if (action == PAGE_QUIT) return true;
            if (action == PAGE_SKIP_CATEGORY) return false;
        }
    }
    return false;
}

static bool page_graph(area_t *areas)
{
    int total = 0;
    for (area_t *a = areas; a; a = a->next) total += a->room_count;

    if (total == 0) {
        printf("\n--- Graph (0 rooms) ---\n");
        return false;
    }
    printf("\n--- Graph Connections (%d rooms) ---\n", total);

    int idx = 0;
    for (area_t *a = areas; a; a = a->next) {
        for (int i = 0; i < a->room_count; i++) {
            printf("\n");
            diku_print_room(&a->rooms[i]);
            page_action_t action = pager_prompt("Graph", ++idx, total);
            if (action == PAGE_QUIT) return true;
            if (action == PAGE_SKIP_CATEGORY) return false;
        }
    }
    return false;
}

static bool page_coords(area_t *areas)
{
    printf("\n--- 3D Coordinates ---\n\n");
    for (area_t *a = areas; a; a = a->next) {
        printf("Area: %s\n", a->name.str ? a->name.str : "(unnamed)");
        diku_print_coordinates(a);
        printf("\n");
    }
    page_action_t action = pager_prompt("Coords", 1, 1);
    if (action == PAGE_QUIT) return true;
    return false;
}

static bool page_symmetry(area_t *areas)
{
    int total = 0, asymmetric = 0;
    for (area_t *a = areas; a; a = a->next) {
        int t = 0, asym = 0;
        diku_check_exit_symmetry(a, &t, &asym);
        total += t;
        asymmetric += asym;
    }

    if (asymmetric == 0) {
        printf("\n--- Exit Symmetry ---\n");
        printf("All exits are symmetric (%d exits checked).\n", total);
        page_action_t action = pager_prompt("Symmetry", 1, 1);
        if (action == PAGE_QUIT) return true;
        return false;
    }

    printf("\n--- Exit Symmetry (%d of %d asymmetric) ---\n", asymmetric, total);

    int current = 0;
    for (area_t *a = areas; a; a = a->next) {
        for (int i = 0; i < a->room_count; i++) {
            room_t *room = &a->rooms[i];
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
    bool load_packages = false;

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
            } else if (strcmp(argv[i], "--packages") == 0) {
                load_packages = true;
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_usage(argv[0]);
                return 0;
            }
        } else {
            filename = argv[i];
        }
    }

    if (!filename) {
        printf("Error: No input path specified\n");
        print_usage(argv[0]);
        return 1;
    }

    area_t *areas = NULL;

    if (is_directory(filename)) {
        diku_set_progress_callback(progress_cb, NULL);
        if (load_packages) {
            areas = diku_load_folder_packages(filename);
        } else {
            areas = diku_load_folder_are(filename);
        }
        diku_set_progress_callback(NULL, NULL);
    } else {
        areas = diku_parse_file(filename);
    }

    if (!areas) {
        printf("Error: Failed to parse %s (errno=%d: %s)\n",
               filename, errno, strerror(errno));
        return 1;
    }

    diku_resolve_graph_global(areas);

    int area_count = 0;
    for (area_t *a = areas; a; a = a->next) area_count++;

    if (area_count > 1) {
        diku_assign_coordinates_multi(areas);
    } else {
        room_t *central = diku_find_central_room(areas);
        if (central) {
            diku_assign_coordinates(areas, central);
        }
    }

    if (verbose) {
        if (!show_rooms && !show_mobiles && !show_objects && !show_graph && !show_coords && !show_symmetry) {
            show_rooms = show_mobiles = show_objects = show_graph = show_coords = show_symmetry = true;
        }

        bool quit = false;
        if (!quit && show_rooms)  quit = page_rooms(areas);
        if (!quit && show_mobiles) quit = page_mobiles(areas);
        if (!quit && show_objects) quit = page_objects(areas);
        if (!quit && show_graph)  quit = page_graph(areas);
        if (!quit && show_coords) quit = page_coords(areas);
        if (!quit && show_symmetry) quit = page_symmetry(areas);
    } else {
        print_overview(areas, filename);
        if (show_symmetry) {
            printf("\n");
            diku_print_exit_symmetry(areas);
        }
    }

    diku_free_all_areas(areas);
    return 0;
}
