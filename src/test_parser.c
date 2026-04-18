#include "diku.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

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

static void render_room_fp(FILE *fp, const void *item, void *user)
{
    (void)user;
    diku_print_room_fp(fp, (const room_t *)item);
}

static void render_mobile_fp(FILE *fp, const void *item, void *user)
{
    (void)user;
    diku_print_mobile_fp(fp, (const mobile_t *)item);
}

static void render_item_fp(FILE *fp, const void *item, void *user)
{
    (void)user;
    diku_print_item_fp(fp, (const item_t *)item);
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

    diku_context_t *ctx = diku_context_create();
    if (!ctx) {
        printf("Error: Failed to create context\n");
        return 1;
    }

    area_t *areas = NULL;

    if (is_directory(filename)) {
        diku_context_set_progress(ctx, progress_cb, NULL);
        if (load_packages) {
            areas = diku_load_folder_packages(ctx, filename);
        } else {
            areas = diku_load_folder_are(ctx, filename);
        }
        diku_context_set_progress(ctx, NULL, NULL);
    } else {
        areas = diku_parse_file(ctx, filename);
    }

    if (!areas) {
        printf("Error: Failed to parse %s (errno=%d: %s)\n",
               filename, errno, strerror(errno));
        diku_context_destroy(ctx);
        return 1;
    }

    diku_resolve_graph_global(ctx, areas);

    int area_count = 0;
    for (area_t *a = areas; a; a = a->next) area_count++;

    if (area_count > 1) {
        diku_assign_coordinates_multi(ctx, areas);
    } else {
        room_t *central = diku_find_central_room(areas);
        if (central) {
            diku_assign_coordinates(ctx, areas, central);
        }
    }

    if (verbose) {
        if (!show_rooms && !show_mobiles && !show_objects && !show_graph && !show_coords && !show_symmetry) {
            show_rooms = show_mobiles = show_objects = show_graph = show_coords = show_symmetry = true;
        }

        bool quit = false;
        diku_pager_t pager = {0};
        pager.out = stdout;

        if (!quit && show_rooms) {
            pager.category = "Room";
            pager.render = render_room_fp;
            quit = diku_pager_run_arealist_rooms(&pager, areas);
        }
        if (!quit && show_mobiles) {
            pager.category = "Mobile";
            pager.render = render_mobile_fp;
            quit = diku_pager_run_arealist_mobiles(&pager, areas);
        }
        if (!quit && show_objects) {
            pager.category = "Object";
            pager.render = render_item_fp;
            quit = diku_pager_run_arealist_items(&pager, areas);
        }
        if (!quit && show_graph) {
            pager.category = "Graph";
            pager.render = render_room_fp;
            quit = diku_pager_run_arealist_rooms(&pager, areas);
        }
        if (!quit && show_coords) {
            printf("\n--- 3D Coordinates ---\n\n");
            for (area_t *a = areas; a; a = a->next) {
                printf("Area: %s\n", a->name.str ? a->name.str : "(unnamed)");
                diku_print_coordinates(a);
                printf("\n");
            }
            diku_page_action_t action = diku_pager_default_prompt("Coords", 1, 1, NULL);
            if (action == DIKU_PAGE_QUIT) quit = true;
        }
        if (!quit && show_symmetry) {
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
            } else {
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
                            if (rev >= 0 && rev < DIKU_MAX_EXITS) return_exit = target->exits[rev];
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
                                diku_page_action_t action = diku_pager_default_prompt("Symmetry", current, asymmetric, NULL);
                                if (action == DIKU_PAGE_QUIT) { quit = true; goto done_sym; }
                                if (action == DIKU_PAGE_SKIP) { quit = true; goto done_sym; }
                            }
                        }
                    }
                }
            }
            if (!quit) {
                diku_page_action_t action = diku_pager_default_prompt("Symmetry", 1, 1, NULL);
                if (action == DIKU_PAGE_QUIT) quit = true;
            }
            done_sym:;
        }
    } else {
        diku_totals_t totals = {0};
        diku_compute_totals(areas, &totals);
        diku_print_totals(stdout, &totals, filename, areas);
        if (show_symmetry) {
            printf("\n");
            diku_print_exit_symmetry(areas);
        }
    }

    diku_free_all_areas(areas);
    diku_context_destroy(ctx);
    return 0;
}
