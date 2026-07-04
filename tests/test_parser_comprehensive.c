#include "diku.h"
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

static int ends_with(const char *s, const char *suffix)
{
    size_t sl = strlen(s), tl = strlen(suffix);
    if (sl < tl) return 0;
    return strcasecmp(s + sl - tl, suffix) == 0;
}

static void collect_package_bases(const char *base_path, char ***bases, int *count)
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
                collect_package_bases(full_path, bases, count);
            } else if (ends_with(entry->d_name, ".wld")) {
                size_t len = strlen(full_path);
                full_path[len - 4] = '\0'; /* strip .wld to get base path */
                *bases = realloc(*bases, (*count + 1) * sizeof(char *));
                (*bases)[*count] = full_path;
                (*count)++;
                full_path = NULL;
            }
        }
        if (full_path) free(full_path);
    }
    closedir(dir);
}

int main(int argc, char *argv[])
{
    bool verbose = false;
    const char *folders[2] = {"./data", "./data2"};
    int folder_count = 2;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else {
            folders[0] = argv[i];
            folder_count = 1;
        }
    }

#ifdef DIKU_VERBOSE
    verbose = true;
#endif

    char **files = NULL;
    int file_count = 0;
    char **packages = NULL;
    int package_count = 0;
    for (int f = 0; f < folder_count; f++) {
        collect_are_files(folders[f], &files, &file_count);
        collect_package_bases(folders[f], &packages, &package_count);
    }

    int count = file_count + package_count;
    if (count == 0) {
        fprintf(stderr, "No .are files or CircleMUD packages found\n");
        return 0;
    }

    qsort(files, file_count, sizeof(char *), compare_strings);
    qsort(packages, package_count, sizeof(char *), compare_strings);

    if (verbose) {
        printf("Found %d .are files and %d CircleMUD packages\n", file_count, package_count);
        printf("Loading until failure...\n\n");
    }

    diku_context_t *ctx = diku_context_create();
    if (!ctx) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }

    int passed = 0;
    int total_rooms = 0, total_mobiles = 0, total_items = 0, total_exits = 0;
    int fmt_counts[DIKU_FMT_CUSTOM + 1] = {0};

    for (int i = 0; i < count; i++) {
        bool is_package = (i >= file_count);
        const char *path = is_package ? packages[i - file_count] : files[i];
        area_t *area = is_package ? diku_parse_package(ctx, path) : diku_parse_file(ctx, path);
        if (!area) {
            printf("\n========================================\n");
            printf("FAILURE on %s %d/%d: %s\n", is_package ? "package" : "file", i + 1, count, path);
            printf("========================================\n");
            printf("Successfully parsed before failure: %d\n", passed);
            printf("Total rooms parsed:    %d\n", total_rooms);
            printf("Total mobiles parsed:  %d\n", total_mobiles);
            printf("Total items parsed:    %d\n", total_items);
            printf("Total exits parsed:    %d\n", total_exits);
            printf("========================================\n");
            for (int j = 0; j < file_count; j++) free(files[j]);
            for (int j = 0; j < package_count; j++) free(packages[j]);
            free(files);
            free(packages);
            diku_context_destroy(ctx);
            return 1;
        }

        passed++;
        total_rooms += area->room_count;
        total_mobiles += area->mobile_count;
        total_items += area->item_count;
        if (area->format >= DIKU_FMT_UNKNOWN && area->format <= DIKU_FMT_CUSTOM)
            fmt_counts[area->format]++;

        for (int r = 0; r < area->room_count; r++) {
            for (int d = 0; d < DIKU_MAX_EXITS; d++) {
                if (area->rooms[r].exits[d]) total_exits++;
            }
        }

        diku_resolve_graph_global(ctx, area);

        if (verbose) {
            printf("  [%d/%d] %-40s  %-8s  R:%4d M:%4d O:%4d\n",
                   i + 1, count, path, diku_format_name(area->format),
                   area->room_count, area->mobile_count, area->item_count);
        } else if ((i + 1) % 10 == 0 || i == count - 1) {
            printf("\r  Parsing %d/%d...", i + 1, count);
            fflush(stdout);
        }

        diku_free_all_areas(area);
    }
    if (!verbose) printf("\r  Parsing %d/%d...\n", count, count);

    printf("\n========================================\n");
    printf("SUCCESS: All %d files parsed!\n", count);
    printf("========================================\n");
    printf("Total rooms parsed:    %d\n", total_rooms);
    printf("Total mobiles parsed:  %d\n", total_mobiles);
    printf("Total items parsed:    %d\n", total_items);
    printf("Total exits parsed:    %d\n", total_exits);
    printf("\nDetected formats:\n");
    for (int f = DIKU_FMT_UNKNOWN; f <= DIKU_FMT_CUSTOM; f++) {
        if (fmt_counts[f] > 0) {
            printf("  %-10s  %d\n", diku_format_name((diku_format_t)f), fmt_counts[f]);
        }
    }
    printf("========================================\n");

    for (int i = 0; i < file_count; i++) free(files[i]);
    for (int i = 0; i < package_count; i++) free(packages[i]);
    free(files);
    free(packages);
    diku_context_destroy(ctx);
    return 0;
}
