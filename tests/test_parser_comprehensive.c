#include "diku_parser.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

static int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

static void collect_are_files(const char *base_path, char ***files, int *count)
{
    DIR *dir = opendir(base_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        size_t path_len = strlen(base_path) + 1 + strlen(entry->d_name) + 1;
        char *full_path = malloc(path_len);
        snprintf(full_path, path_len, "%s/%s", base_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                collect_are_files(full_path, files, count);
            } else {
                size_t len = strlen(entry->d_name);
                if (len > 4 && strcasecmp(entry->d_name + len - 4, ".are") == 0) {
                    *files = realloc(*files, (*count + 1) * sizeof(char *));
                    (*files)[*count] = full_path;
                    (*count)++;
                    full_path = NULL; /* ownership transferred */
                }
            }
        }
        if (full_path) free(full_path);
    }
    closedir(dir);
}

int main(int argc, char *argv[])
{
    const char *folders[2] = {"./data", "./data2"};
    int folder_count = 2;
    if (argc > 1) {
        folders[0] = argv[1];
        folder_count = 1;
    }

    char **files = NULL;
    int count = 0;
    for (int f = 0; f < folder_count; f++) {
        collect_are_files(folders[f], &files, &count);
    }

    if (count == 0) {
        fprintf(stderr, "No .are files found\n");
        return 0;
    }

    qsort(files, count, sizeof(char *), compare_strings);

    printf("Found %d .are files\n", count);
    printf("Loading until failure...\n\n");

    diku_context_t *ctx = diku_context_create();
    if (!ctx) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }

    int passed = 0;
    int total_rooms = 0, total_mobiles = 0, total_items = 0, total_exits = 0;

    for (int i = 0; i < count; i++) {
        area_t *area = diku_parse_file(ctx, files[i]);
        if (!area) {
            printf("\n========================================\n");
            printf("FAILURE on file %d/%d: %s\n", i + 1, count, files[i]);
            printf("========================================\n");
            printf("Successfully parsed before failure: %d files\n", passed);
            printf("Total rooms parsed:    %d\n", total_rooms);
            printf("Total mobiles parsed:  %d\n", total_mobiles);
            printf("Total items parsed:    %d\n", total_items);
            printf("Total exits parsed:    %d\n", total_exits);
            printf("========================================\n");
            for (int j = 0; j < count; j++) free(files[j]);
            free(files);
            diku_context_destroy(ctx);
            return 1;
        }

        passed++;
        total_rooms += area->room_count;
        total_mobiles += area->mobile_count;
        total_items += area->item_count;

        for (int r = 0; r < area->room_count; r++) {
            for (int d = 0; d < DIKU_MAX_EXITS; d++) {
                if (area->rooms[r].exits[d]) total_exits++;
            }
        }

        diku_resolve_graph_global(ctx, area);

        if ((i + 1) % 10 == 0 || i == count - 1) {
            printf("  [%d/%d] %s  ->  R:%d M:%d O:%d\n",
                   i + 1, count, files[i],
                   area->room_count, area->mobile_count, area->item_count);
        }

        diku_free_all_areas(area);
    }

    printf("\n========================================\n");
    printf("SUCCESS: All %d files parsed!\n", count);
    printf("========================================\n");
    printf("Total rooms parsed:    %d\n", total_rooms);
    printf("Total mobiles parsed:  %d\n", total_mobiles);
    printf("Total items parsed:    %d\n", total_items);
    printf("Total exits parsed:    %d\n", total_exits);
    printf("========================================\n");

    for (int i = 0; i < count; i++) free(files[i]);
    free(files);
    diku_context_destroy(ctx);
    return 0;
}
