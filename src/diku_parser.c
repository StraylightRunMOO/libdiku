#define MEMENTO_IMPLEMENTATION
#define DIKU_PARSER_IMPLEMENTATION
#include "diku_parser.h"
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <strings.h>

/* ------------------------------------------------------------------ */
/* Global state                                                       */
/* ------------------------------------------------------------------ */
diku_global_state_t diku_global = {0, 0, 0, 0, 0, 0, 0};

/* ------------------------------------------------------------------ */
/* Progress callbacks                                                 */
/* ------------------------------------------------------------------ */
static diku_progress_cb_t g_progress_cb = NULL;
static void *g_progress_user = NULL;

void diku_set_progress_callback(diku_progress_cb_t cb, void *user) {
    g_progress_cb = cb;
    g_progress_user = user;
}

static void report_progress(const char *op, int cur, int total, const char *detail) {
    if (g_progress_cb) {
        g_progress_cb(op, cur, total, detail ? detail : "", g_progress_user);
    }
}

/* Arena wrappers — implemented via DIKU_PARSER_IMPLEMENTATION in diku/arena.h */

/* Lexer — implemented via DIKU_PARSER_IMPLEMENTATION in diku/lexer.h */

/* Section parsers — implemented via DIKU_PARSER_IMPLEMENTATION in diku/sections.h */

area_t *diku_parse_package_files(const char *wld, const char *mob, const char *obj, const char *zon) {
    memento_arena_t *arena = diku_arena_create();
    if (!arena) return NULL;
    
    area_t *area = (area_t *)diku_arena_alloc_aligned(arena, sizeof(area_t), 64);
    if (!area) {
        diku_arena_free_all(arena);
        return NULL;
    }
    memset(area, 0, sizeof(area_t));
    area->arena = arena;
    
    bool has_data = false;
    
    {
        diku_lexer_t file_lex;
        if (diku_lexer_init_file(&file_lex, wld)) {
            area->filename = diku_arena_strndup(arena, wld, strlen(wld));
            if (diku_parse_wld(&file_lex, area)) has_data = true;
            diku_lexer_cleanup(&file_lex);
        }
    }
    
    {
        diku_lexer_t file_lex;
        if (diku_lexer_init_file(&file_lex, mob)) {
            if (diku_parse_mob(&file_lex, area)) has_data = true;
            diku_lexer_cleanup(&file_lex);
        }
    }
    
    {
        diku_lexer_t file_lex;
        if (diku_lexer_init_file(&file_lex, obj)) {
            if (diku_parse_obj(&file_lex, area)) has_data = true;
            diku_lexer_cleanup(&file_lex);
        }
    }
    
    {
        diku_lexer_t file_lex;
        if (diku_lexer_init_file(&file_lex, zon)) {
            if (diku_parse_zon(&file_lex, area)) has_data = true;
            diku_lexer_cleanup(&file_lex);
        }
    }
    
    if (!has_data) {
        diku_free_area(area);
        return NULL;
    }
    
    diku_build_vnum_hash(area);
    return area;
}

area_t *diku_parse_package(const char *base_path) {
    size_t len = strlen(base_path);
    char *wld = (char *)malloc(len + 5);
    char *mob = (char *)malloc(len + 5);
    char *obj = (char *)malloc(len + 5);
    char *zon = (char *)malloc(len + 5);
    
    if (!wld || !mob || !obj || !zon) {
        free(wld); free(mob); free(obj); free(zon);
        return NULL;
    }
    
    sprintf(wld, "%s.wld", base_path);
    sprintf(mob, "%s.mob", base_path);
    sprintf(obj, "%s.obj", base_path);
    sprintf(zon, "%s.zon", base_path);
    
    area_t *area = diku_parse_package_files(wld, mob, obj, zon);
    
    free(wld); free(mob); free(obj); free(zon);
    return area;
}

area_t *diku_load_folder_are(const char *folder_path) {
    DIR *dir = opendir(folder_path);
    if (!dir) return NULL;
    
    struct dirent *entry;
    int total = 0;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcasecmp(entry->d_name + len - 4, ".are") == 0) {
            total++;
        }
    }
    rewinddir(dir);
    
    area_t *head = NULL;
    area_t *tail = NULL;
    int current = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len <= 4 || strcasecmp(entry->d_name + len - 4, ".are") != 0) continue;
        
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", folder_path, entry->d_name);
        
        report_progress("parse_are", current, total, path);
        
        area_t *area = diku_parse_file(path);
        if (area) {
            if (!head) {
                head = tail = area;
            } else {
                tail->next = area;
                tail = area;
            }
        }
        current++;
    }
    
    closedir(dir);
    report_progress("parse_are", total, total, "done");
    return head;
}

area_t *diku_load_folder_packages(const char *folder_path) {
    DIR *dir = opendir(folder_path);
    if (!dir) return NULL;
    
    struct dirent *entry;
    int total = 0;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcasecmp(entry->d_name + len - 4, ".wld") == 0) {
            total++;
        }
    }
    rewinddir(dir);
    
    area_t *head = NULL;
    area_t *tail = NULL;
    int current = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len <= 4 || strcasecmp(entry->d_name + len - 4, ".wld") != 0) continue;
        
        char base[4096];
        snprintf(base, sizeof(base), "%s/%.*s", folder_path, (int)(len - 4), entry->d_name);
        
        report_progress("parse_package", current, total, base);
        
        area_t *area = diku_parse_package(base);
        if (area) {
            if (!head) {
                head = tail = area;
            } else {
                tail->next = area;
                tail = area;
            }
        }
        current++;
    }
    
    closedir(dir);
    report_progress("parse_package", total, total, "done");
    return head;
}

/* ------------------------------------------------------------------ */
/* Store raw section (for resets, shops, etc)                         */
/* ------------------------------------------------------------------ */

static char **store_raw_section(diku_lexer_t *lex, memento_arena_t *arena, int *line_count) {
    char **lines = NULL;
    int count = 0;
    
    char buf[4096];
    while (1) {
        long line_pos = diku_lexer_tell(lex);
        if (!diku_lexer_read_line(lex, buf, sizeof(buf))) break;
        /* Check for end of section */
        if (buf[0] == '0' && (buf[1] == '\n' || buf[1] == '\r' || buf[1] == '\0')) {
            break;
        }
        if (buf[0] == '#') {
            /* Next section - seek back */
            diku_lexer_seek(lex, line_pos);
            break;
        }
        
        /* Store line */
        count++;
        lines = (char **)realloc(lines, count * sizeof(char *));
        if (!lines) return NULL;
        
        /* Remove newline */
        size_t len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
            buf[--len] = '\0';
        
        lines[count - 1] = diku_arena_strdup(arena, buf);
    }
    
    *line_count = count;
    return lines;
}

/* ------------------------------------------------------------------ */
/* Main file parser                                                   */
/* ------------------------------------------------------------------ */

area_t *diku_parse_lexer(diku_lexer_t *lex, const char *filename) {
    if (!lex) return NULL;
    
    memento_arena_t *arena = diku_arena_create();
    if (!arena) return NULL;
    
    area_t *area = diku_parse_area_header(lex, arena);
    if (!area) {
        diku_arena_free_all(arena);
        return NULL;
    }
    
    area->filename = diku_arena_strndup(arena, filename, strlen(filename));
    
    /* Parse sections */
    char buf[256];
    while (diku_lexer_read_word(lex, buf, 255)) {
        if (buf[0] != '#') {
            /* Not a section marker */
            continue;
        }
        
        const char *section = buf + 1;
        
        if (strcmp(section, "ROOMS") == 0) {
            /* Parse rooms */
            int room_capacity = 16;
            area->rooms = (room_t *)diku_arena_alloc_aligned(arena, sizeof(room_t) * room_capacity, 64);
            if (!area->rooms) {
                fprintf(stderr, "ERROR: Failed to allocate rooms array\n");
                continue;
            }
            
            int vnum;
            while (1) {
                diku_lexer_skip_ws(lex);
                long pos = diku_lexer_tell(lex);
                
                /* Check for end of rooms */
                int c = diku_lexer_getc(lex);
                if (c == '#') {
                    char next[16];
                    if (diku_lexer_read_word(lex, next, 16)) {
                        if (strcmp(next, "0") == 0) {
                            break;  /* End of rooms */
                        } else {
                            /* It's a room vnum - skip the # and position after it */
                            diku_lexer_seek(lex, pos + 1);
                        }
                    }
                } else if (c == '0') {
                    break;  /* End of rooms */
                } else if (c == EOF) {
                    break;
                } else {
                    diku_lexer_seek(lex, pos);
                    diku_lexer_skip_line(lex);
                    continue;
                }
                
                /* Parse room */
                room_t *room = diku_parse_room(lex, arena, &vnum);
                if (!room) {
                    fprintf(stderr, "diku_parse_room NULL at pos %ld\n", diku_lexer_tell(lex));
                    break;
                }
                fprintf(stderr, "ROOM #%d %s\n", vnum, room->name.str ? room->name.str : "");
                
                /* Grow array if needed */
                if (area->room_count >= room_capacity) {
                    room_capacity *= 2;
                    room_t *new_rooms = (room_t *)diku_arena_alloc_aligned(arena, sizeof(room_t) * room_capacity, 64);
                    memcpy(new_rooms, area->rooms, area->room_count * sizeof(room_t));
                    area->rooms = new_rooms;
                }
                
                memcpy(&area->rooms[area->room_count++], room, sizeof(room_t));
            }
            
        } else if (strcmp(section, "MOBILES") == 0 || strcmp(section, "MOBILE") == 0) {
            /* Parse mobiles */
            int mob_capacity = 16;
            area->mobiles = (mobile_t *)diku_arena_alloc_aligned(arena, sizeof(mobile_t) * mob_capacity, 64);
            
            int vnum;
            while (1) {
                diku_lexer_skip_ws(lex);
                long pos = diku_lexer_tell(lex);
                
                int c = diku_lexer_getc(lex);
                if (c == '#') {
                    char next[16];
                    if (diku_lexer_read_word(lex, next, 16)) {
                        if (strcmp(next, "0") == 0) {
                            break;
                        } else {
                            diku_lexer_seek(lex, pos + 1);
                        }
                    }
                } else if (c == '0') {
                    break;
                } else if (c == EOF) {
                    break;
                } else {
                    diku_lexer_seek(lex, pos);
                    diku_lexer_skip_line(lex);
                    continue;
                }
                
                mobile_t *mob = diku_parse_mobile(lex, arena, &vnum);
                if (!mob) break;
                
                if (area->mobile_count >= mob_capacity) {
                    mob_capacity *= 2;
                    mobile_t *new_mobs = (mobile_t *)diku_arena_alloc_aligned(arena, sizeof(mobile_t) * mob_capacity, 64);
                    memcpy(new_mobs, area->mobiles, area->mobile_count * sizeof(mobile_t));
                    area->mobiles = new_mobs;
                }
                
                memcpy(&area->mobiles[area->mobile_count++], mob, sizeof(mobile_t));
            }
            
        } else if (strcmp(section, "OBJECTS") == 0 || strcmp(section, "OBJECT") == 0 || 
                   strcmp(section, "OBJ") == 0) {

            /* Parse items */
            int item_capacity = 16;
            area->items = (item_t *)diku_arena_alloc_aligned(arena, sizeof(item_t) * item_capacity, 64);
            
            int vnum;
            while (1) {
                diku_lexer_skip_ws(lex);
                long pos = diku_lexer_tell(lex);
                
                int c = diku_lexer_getc(lex);
                if (c == '#') {
                    char next[16];
                    if (diku_lexer_read_word(lex, next, 16)) {
                        if (strcmp(next, "0") == 0) {
                            break;
                        } else {
                            diku_lexer_seek(lex, pos + 1);
                        }
                    }
                } else if (c == '0') {
                    break;
                } else if (c == EOF) {
                    break;
                } else {
                    diku_lexer_seek(lex, pos);
                    diku_lexer_skip_line(lex);
                    continue;
                }
                
                item_t *item = diku_parse_item(lex, arena, &vnum);

                if (!item) {
                    break;
                }
                
                if (area->item_count >= item_capacity) {
                    item_capacity *= 2;
                    item_t *new_items = (item_t *)diku_arena_alloc_aligned(arena, sizeof(item_t) * item_capacity, 64);
                    memcpy(new_items, area->items, area->item_count * sizeof(item_t));
                    area->items = new_items;
                }
                
                memcpy(&area->items[area->item_count++], item, sizeof(item_t));
            }
            
        } else if (strcmp(section, "HELPS") == 0) {
            area->helps_raw = store_raw_section(lex, arena, &area->helps_line_count);
            
        } else if (strcmp(section, "RESETS") == 0 || strcmp(section, "RESET") == 0) {
            area->resets_raw = store_raw_section(lex, arena, &area->resets_line_count);
            
        } else if (strcmp(section, "SHOPS") == 0 || strcmp(section, "SHOP") == 0) {
            area->shops_raw = store_raw_section(lex, arena, &area->shops_line_count);
            
        } else if (strcmp(section, "SPECIALS") == 0 || strcmp(section, "SPECIAL") == 0) {
            area->specials_raw = store_raw_section(lex, arena, &area->specials_line_count);
            
        } else if (strcmp(section, "OBJFUNS") == 0 || strcmp(section, "OBJFUN") == 0) {
            area->objfuns_raw = store_raw_section(lex, arena, &area->objfuns_line_count);
            
        } else if (strcmp(section, "$") == 0) {
            /* End of file */
            break;
            
        } else {
            /* Unknown section - store as extra */
            area->extra_section_count++;
            /* Use malloc for this since it's dynamic */
            /* For now, just skip the section */
            store_raw_section(lex, arena, &(int){0});
        }
    }
    
    diku_build_vnum_hash(area);

    /* Parse resets into room contents */
    diku_parse_resets(area);
    
    return area;
}

/* Reset parser — implemented via DIKU_PARSER_IMPLEMENTATION in diku/resets.h */

area_t *diku_parse_file(const char *filename) {
    diku_lexer_t lex;
    if (diku_lexer_init_file(&lex, filename)) {
        area_t *area = diku_parse_lexer(&lex, filename);
        diku_lexer_cleanup(&lex);
        if (area) return area;
    }
    
    /* If not a regular file, try as a multi-file package base path */
    return diku_parse_package(filename);
}

/* ------------------------------------------------------------------ */
/* Graph resolution - link exits to rooms                             */
/* ------------------------------------------------------------------ */

/* diku_find_room — implemented via DIKU_PARSER_IMPLEMENTATION in diku/find.h */

void diku_resolve_graph(area_t **areas, int area_count) {
    /* Build global room lookup */
    for (int a = 0; a < area_count; a++) {
        for (int r = 0; r < areas[a]->room_count; r++) {
            room_t *room = &areas[a]->rooms[r];
            
            for (int d = 0; d < DIKU_MAX_EXITS; d++) {
                exit_t *exit = room->exits[d];
                if (!exit) continue;
                
                /* Find target room */
                exit->to_room = NULL;
                
                /* Search all areas */
                for (int aa = 0; aa < area_count; aa++) {
                    exit->to_room = diku_find_room(areas[aa], exit->to_vnum);
                    if (exit->to_room) break;
                }
            }
        }
    }
}

void diku_resolve_graph_global(area_t *areas) {
    if (!areas) return;
    
    /* Count areas in linked list */
    int count = 0;
    for (area_t *a = areas; a; a = a->next) count++;
    
    /* Build array of area pointers */
    area_t **area_array = (area_t **)malloc(count * sizeof(area_t *));
    int i = 0;
    for (area_t *a = areas; a; a = a->next) {
        area_array[i++] = a;
    }
    
    diku_resolve_graph(area_array, count);
    free(area_array);
}

/* ------------------------------------------------------------------ */
/* 3D Coordinate assignment                                           */
/* ------------------------------------------------------------------ */

/* Find the "central" room using multiple heuristics */
room_t *diku_find_central_room(area_t *area) {
    if (!area || area->room_count == 0) return NULL;
    if (area->room_count == 1) return &area->rooms[0];
    
    /* First pass: calculate degree centrality (most connected room) */
    int *degree = (int *)calloc(area->room_count, sizeof(int));
    int *eccentricity = (int *)calloc(area->room_count, sizeof(int));
    
    /* Count connections for each room */
    for (int i = 0; i < area->room_count; i++) {
        for (int d = 0; d < DIKU_MAX_EXITS; d++) {
            if (area->rooms[i].exits[d] && area->rooms[i].exits[d]->to_vnum > 0) {
                degree[i]++;
            }
        }
    }
    
    /* Find room with highest degree */
    int max_degree = 0;
    int max_degree_idx = 0;
    for (int i = 0; i < area->room_count; i++) {
        if (degree[i] > max_degree) {
            max_degree = degree[i];
            max_degree_idx = i;
        }
    }
    
    /* If there's a room with significantly more connections, use it */
    if (max_degree >= 4) {
        free(degree);
        free(eccentricity);
        return &area->rooms[max_degree_idx];
    }
    
    /* Second pass: find room closest to center of vnum range */
    int min_vnum = INT_MAX, max_vnum = INT_MIN;
    for (int i = 0; i < area->room_count; i++) {
        if (area->rooms[i].vnum < min_vnum) min_vnum = area->rooms[i].vnum;
        if (area->rooms[i].vnum > max_vnum) max_vnum = area->rooms[i].vnum;
    }
    int mid_vnum = (min_vnum + max_vnum) / 2;
    
    int closest_to_mid = 0;
    int min_vnum_dist = abs(area->rooms[0].vnum - mid_vnum);
    for (int i = 1; i < area->room_count; i++) {
        int dist = abs(area->rooms[i].vnum - mid_vnum);
        if (dist < min_vnum_dist) {
            min_vnum_dist = dist;
            closest_to_mid = i;
        }
    }
    
    free(degree);
    free(eccentricity);
    
    return &area->rooms[closest_to_mid];
}

/* Hash table for coordinate occupancy checking */
typedef struct coord_entry {
    int x, y, z;
    room_t *room;
    struct coord_entry *next;
} coord_entry_t;

static coord_entry_t **coord_hash = NULL;
static int coord_hash_size = 0;

static void coord_hash_init(int size) {
    coord_hash_size = size;
    coord_hash = (coord_entry_t **)calloc(size, sizeof(coord_entry_t *));
}

static void coord_hash_free(void) {
    if (!coord_hash) return;
    for (int i = 0; i < coord_hash_size; i++) {
        coord_entry_t *e = coord_hash[i];
        while (e) {
            coord_entry_t *next = e->next;
            free(e);
            e = next;
        }
    }
    free(coord_hash);
    coord_hash = NULL;
}

static unsigned coord_hash_key(int x, int y, int z) {
    /* Simple hash function for 3D coordinates */
    unsigned h = ((unsigned)(x * 73856093) ^ 
                  (unsigned)(y * 19349663) ^ 
                  (unsigned)(z * 83492791));
    return h % coord_hash_size;
}

static bool coord_hash_check(int x, int y, int z, room_t *exclude) {
    if (!coord_hash) return false;
    
    unsigned key = coord_hash_key(x, y, z);
    coord_entry_t *e = coord_hash[key];
    
    while (e) {
        if (e->x == x && e->y == y && e->z == z) {
            if (e->room != exclude) return true;
        }
        e = e->next;
    }
    return false;
}

static void coord_hash_insert(int x, int y, int z, room_t *room) {
    if (!coord_hash) return;
    
    unsigned key = coord_hash_key(x, y, z);
    coord_entry_t *e = (coord_entry_t *)malloc(sizeof(coord_entry_t));
    e->x = x; e->y = y; e->z = z;
    e->room = room;
    e->next = coord_hash[key];
    coord_hash[key] = e;
}

/* BFS-based coordinate assignment with collision detection */
void diku_assign_coordinates(area_t *area, room_t *root) {
    if (!area || !root) return;
    
    /* Reset all coordinate assignments */
    for (int i = 0; i < area->room_count; i++) {
        area->rooms[i].coord_assigned = false;
        area->rooms[i].coord.x = area->rooms[i].coord.y = area->rooms[i].coord.z = 0;
    }
    
    /* Initialize coordinate hash */
    coord_hash_init(area->room_count * 2 + 16);
    
    /* BFS queue */
    room_t **queue = (room_t **)malloc(area->room_count * sizeof(room_t *));
    int qhead = 0, qtail = 0;
    
    /* Start with root at origin */
    root->coord.x = root->coord.y = root->coord.z = 0;
    root->coord_assigned = true;
    coord_hash_insert(0, 0, 0, root);
    queue[qtail++] = root;
    
    /* Track bounds */
    diku_global.min_x = diku_global.max_x = 0;
    diku_global.min_y = diku_global.max_y = 0;
    diku_global.min_z = diku_global.max_z = 0;
    
    while (qhead < qtail) {
        room_t *current = queue[qhead++];
        
        /* Process all exits */
        for (int d = 0; d < DIKU_MAX_EXITS; d++) {
            exit_t *exit = current->exits[d];
            if (!exit || !exit->to_room) continue;
            
            room_t *next = exit->to_room;
            if (next->coord_assigned) continue;
            
            /* Calculate desired position */
            int nx = current->coord.x + dir_offset[d][0];
            int ny = current->coord.y + dir_offset[d][1];
            int nz = current->coord.z + dir_offset[d][2];
            
            /* IN/OUT share the source room's coordinate */
            if (d != DIR_IN && d != DIR_OUT) {
                /* Check for collision and resolve */
                int attempts = 0;
                const int max_attempts = 26;  /* Check all neighbors */
                
                while (coord_hash_check(nx, ny, nz, next) && attempts < max_attempts) {
                    /* Try spiral pattern around desired position */
                    static const int spiral[26][3] = {
                        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1},
                        {1,1,0}, {1,-1,0}, {-1,1,0}, {-1,-1,0},
                        {1,0,1}, {1,0,-1}, {-1,0,1}, {-1,0,-1},
                        {0,1,1}, {0,1,-1}, {0,-1,1}, {0,-1,-1},
                        {1,1,1}, {1,1,-1}, {1,-1,1}, {1,-1,-1},
                        {-1,1,1}, {-1,1,-1}, {-1,-1,1}, {-1,-1,-1}
                    };
                    
                    if (attempts < 26) {
                        nx = current->coord.x + dir_offset[d][0] + spiral[attempts][0];
                        ny = current->coord.y + dir_offset[d][1] + spiral[attempts][1];
                        nz = current->coord.z + dir_offset[d][2] + spiral[attempts][2];
                    }
                    attempts++;
                }
                
                /* If still colliding, expand outward */
                if (coord_hash_check(nx, ny, nz, next)) {
                    int radius = 2;
                    while (coord_hash_check(nx, ny, nz, next) && radius < 100) {
                        for (int dx = -radius; dx <= radius && coord_hash_check(nx, ny, nz, next); dx++) {
                            for (int dy = -radius; dy <= radius && coord_hash_check(nx, ny, nz, next); dy++) {
                                for (int dz = -radius; dz <= radius && coord_hash_check(nx, ny, nz, next); dz++) {
                                    int tx = current->coord.x + dir_offset[d][0] + dx;
                                    int ty = current->coord.y + dir_offset[d][1] + dy;
                                    int tz = current->coord.z + dir_offset[d][2] + dz;
                                    if (!coord_hash_check(tx, ty, tz, next)) {
                                        nx = tx; ny = ty; nz = tz;
                                    }
                                }
                            }
                        }
                        radius++;
                    }
                }
            }
            
            /* Assign coordinate */
            next->coord.x = nx;
            next->coord.y = ny;
            next->coord.z = nz;
            next->coord_assigned = true;
            coord_hash_insert(nx, ny, nz, next);
            
            /* Update bounds */
            if (nx < diku_global.min_x) diku_global.min_x = nx;
            if (nx > diku_global.max_x) diku_global.max_x = nx;
            if (ny < diku_global.min_y) diku_global.min_y = ny;
            if (ny > diku_global.max_y) diku_global.max_y = ny;
            if (nz < diku_global.min_z) diku_global.min_z = nz;
            if (nz > diku_global.max_z) diku_global.max_z = nz;
            
            /* Add to queue */
            queue[qtail++] = next;
        }
    }
    
    free(queue);
    coord_hash_free();
}

void diku_assign_coordinates_all(area_t *areas) {
    if (!areas) return;
    
    for (area_t *area = areas; area; area = area->next) {
        room_t *central = diku_find_central_room(area);
        if (central) {
            diku_assign_coordinates(area, central);
        }
    }
}

/* Multi-area BFS coordinate assignment (cross-area consistent) */
void diku_assign_coordinates_multi(area_t *areas) {
    if (!areas) return;
    
    /* Count total rooms across all areas */
    int total_rooms = 0;
    for (area_t *area = areas; area; area = area->next) {
        total_rooms += area->room_count;
    }
    if (total_rooms == 0) return;
    
    /* Reset all coordinate assignments */
    for (area_t *area = areas; area; area = area->next) {
        for (int i = 0; i < area->room_count; i++) {
            area->rooms[i].coord_assigned = false;
            area->rooms[i].coord.x = area->rooms[i].coord.y = area->rooms[i].coord.z = 0;
        }
    }
    
    /* Initialize coordinate hash */
    coord_hash_init(total_rooms * 2 + 16);
    
    /* BFS queue */
    room_t **queue = (room_t **)malloc(total_rooms * sizeof(room_t *));
    int qhead = 0, qtail = 0;
    
    /* Find root room - prefer central room of first area */
    room_t *root = diku_find_central_room(areas);
    if (!root && areas->room_count > 0) root = &areas->rooms[0];
    if (!root) { free(queue); coord_hash_free(); return; }
    
    /* Start with root at origin */
    root->coord.x = root->coord.y = root->coord.z = 0;
    root->coord_assigned = true;
    coord_hash_insert(0, 0, 0, root);
    queue[qtail++] = root;
    
    /* Track bounds */
    diku_global.min_x = diku_global.max_x = 0;
    diku_global.min_y = diku_global.max_y = 0;
    diku_global.min_z = diku_global.max_z = 0;
    
    while (qhead < qtail) {
        room_t *current = queue[qhead++];
        
        /* Process all exits */
        for (int d = 0; d < DIKU_MAX_EXITS; d++) {
            exit_t *exit = current->exits[d];
            if (!exit || !exit->to_room) continue;
            
            room_t *next = exit->to_room;
            if (next->coord_assigned) continue;
            
            /* Calculate desired position */
            int nx = current->coord.x + dir_offset[d][0];
            int ny = current->coord.y + dir_offset[d][1];
            int nz = current->coord.z + dir_offset[d][2];
            
            /* IN/OUT share the source room's coordinate */
            if (d != DIR_IN && d != DIR_OUT) {
                /* Check for collision and resolve */
                int attempts = 0;
                const int max_attempts = 26;  /* Check all neighbors */
                
                while (coord_hash_check(nx, ny, nz, next) && attempts < max_attempts) {
                    /* Try spiral pattern around desired position */
                    static const int spiral[26][3] = {
                        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1},
                        {1,1,0}, {1,-1,0}, {-1,1,0}, {-1,-1,0},
                        {1,0,1}, {1,0,-1}, {-1,0,1}, {-1,0,-1},
                        {0,1,1}, {0,1,-1}, {0,-1,1}, {0,-1,-1},
                        {1,1,1}, {1,1,-1}, {1,-1,1}, {1,-1,-1},
                        {-1,1,1}, {-1,1,-1}, {-1,-1,1}, {-1,-1,-1}
                    };
                    
                    if (attempts < 26) {
                        nx = current->coord.x + dir_offset[d][0] + spiral[attempts][0];
                        ny = current->coord.y + dir_offset[d][1] + spiral[attempts][1];
                        nz = current->coord.z + dir_offset[d][2] + spiral[attempts][2];
                    }
                    attempts++;
                }
                
                /* If still colliding, expand outward */
                if (coord_hash_check(nx, ny, nz, next)) {
                    int radius = 2;
                    while (coord_hash_check(nx, ny, nz, next) && radius < 100) {
                        for (int dx = -radius; dx <= radius && coord_hash_check(nx, ny, nz, next); dx++) {
                            for (int dy = -radius; dy <= radius && coord_hash_check(nx, ny, nz, next); dy++) {
                                for (int dz = -radius; dz <= radius && coord_hash_check(nx, ny, nz, next); dz++) {
                                    int tx = current->coord.x + dir_offset[d][0] + dx;
                                    int ty = current->coord.y + dir_offset[d][1] + dy;
                                    int tz = current->coord.z + dir_offset[d][2] + dz;
                                    if (!coord_hash_check(tx, ty, tz, next)) {
                                        nx = tx; ny = ty; nz = tz;
                                    }
                                }
                            }
                        }
                        radius++;
                    }
                }
            }
            
            /* Assign coordinate */
            next->coord.x = nx;
            next->coord.y = ny;
            next->coord.z = nz;
            next->coord_assigned = true;
            coord_hash_insert(nx, ny, nz, next);
            
            /* Update bounds */
            if (nx < diku_global.min_x) diku_global.min_x = nx;
            if (nx > diku_global.max_x) diku_global.max_x = nx;
            if (ny < diku_global.min_y) diku_global.min_y = ny;
            if (ny > diku_global.max_y) diku_global.max_y = ny;
            if (nz < diku_global.min_z) diku_global.min_z = nz;
            if (nz > diku_global.max_z) diku_global.max_z = nz;
            
            /* Add to queue */
            queue[qtail++] = next;
        }
    }
    
    free(queue);
    coord_hash_free();
}

bool diku_coord_occupied(area_t *area, int x, int y, int z, room_t *exclude) {
    if (!area) return false;
    
    for (int i = 0; i < area->room_count; i++) {
        if (&area->rooms[i] == exclude) continue;
        if (area->rooms[i].coord_assigned &&
            area->rooms[i].coord.x == x &&
            area->rooms[i].coord.y == y &&
            area->rooms[i].coord.z == z) {
            return true;
        }
    }
    return false;
}

room_t *diku_room_at_coord(area_t *area, int x, int y, int z) {
    if (!area) return NULL;
    
    for (int i = 0; i < area->room_count; i++) {
        if (area->rooms[i].coord_assigned &&
            area->rooms[i].coord.x == x &&
            area->rooms[i].coord.y == y &&
            area->rooms[i].coord.z == z) {
            return &area->rooms[i];
        }
    }
    return NULL;
}

void diku_center_coordinates(area_t *area) {
    if (!area || area->room_count == 0) return;
    
    /* Calculate centroid */
    long sum_x = 0, sum_y = 0, sum_z = 0;
    int count = 0;
    
    for (int i = 0; i < area->room_count; i++) {
        if (area->rooms[i].coord_assigned) {
            sum_x += area->rooms[i].coord.x;
            sum_y += area->rooms[i].coord.y;
            sum_z += area->rooms[i].coord.z;
            count++;
        }
    }
    
    if (count == 0) return;
    
    int offset_x = (int)(sum_x / count);
    int offset_y = (int)(sum_y / count);
    int offset_z = (int)(sum_z / count);
    
    /* Shift all coordinates */
    for (int i = 0; i < area->room_count; i++) {
        if (area->rooms[i].coord_assigned) {
            area->rooms[i].coord.x -= offset_x;
            area->rooms[i].coord.y -= offset_y;
            area->rooms[i].coord.z -= offset_z;
        }
    }
}

void diku_get_coord_bounds(area_t *area, int *min_x, int *max_x, 
                           int *min_y, int *max_y, int *min_z, int *max_z) {
    if (!area || area->room_count == 0) {
        *min_x = *max_x = *min_y = *max_y = *min_z = *max_z = 0;
        return;
    }
    
    *min_x = *max_x = 0;
    *min_y = *max_y = 0;
    *min_z = *max_z = 0;
    
    bool first = true;
    for (int i = 0; i < area->room_count; i++) {
        if (!area->rooms[i].coord_assigned) continue;
        
        if (first) {
            *min_x = *max_x = area->rooms[i].coord.x;
            *min_y = *max_y = area->rooms[i].coord.y;
            *min_z = *max_z = area->rooms[i].coord.z;
            first = false;
        } else {
            if (area->rooms[i].coord.x < *min_x) *min_x = area->rooms[i].coord.x;
            if (area->rooms[i].coord.x > *max_x) *max_x = area->rooms[i].coord.x;
            if (area->rooms[i].coord.y < *min_y) *min_y = area->rooms[i].coord.y;
            if (area->rooms[i].coord.y > *max_y) *max_y = area->rooms[i].coord.y;
            if (area->rooms[i].coord.z < *min_z) *min_z = area->rooms[i].coord.z;
            if (area->rooms[i].coord.z > *max_z) *max_z = area->rooms[i].coord.z;
        }
    }
}

/* diku_find_mobile, diku_find_item, diku_find_room_global — in diku/find.h */

/* ------------------------------------------------------------------ */
/* Cleanup functions                                                  */
/* ------------------------------------------------------------------ */

void diku_free_area(area_t *area) {
    if (!area) return;
    
    /* Save pointer to hash table (not in arena) */
    void *hash_table = area->rooms_by_vnum;
    memento_arena_t *arena = area->arena;
    
    /* Free the hash table first */
    if (hash_table) {
        free(hash_table);
    }
    
    /* Free the arena - this frees everything else including the area struct */
    if (arena) {
        diku_arena_free_all(arena);
    }
}

void diku_free_all_areas(area_t *areas) {
    while (areas) {
        area_t *next = areas->next;
        diku_free_area(areas);
        areas = next;
    }
}

/* diku_dir_name*, diku_reverse_dir, diku_has_exit, diku_get_exit_vnum,
   diku_count_exits — moved to static inline in diku/util.h */

/* ------------------------------------------------------------------ */
/* Exit symmetry check                                                */
/* ------------------------------------------------------------------ */

int diku_check_exit_symmetry(const area_t *area, int *out_total, int *out_asymmetric) {
    if (!area) {
        if (out_total) *out_total = 0;
        if (out_asymmetric) *out_asymmetric = 0;
        return 0;
    }
    
    int total = 0;
    int asymmetric = 0;
    
    for (int i = 0; i < area->room_count; i++) {
        room_t *room = &area->rooms[i];
        for (int d = 0; d < DIKU_MAX_EXITS; d++) {
            exit_t *exit = room->exits[d];
            if (!exit || !exit->to_room) continue;
            total++;
            
            int rev = diku_reverse_dir(d);
            room_t *target = exit->to_room;
            exit_t *return_exit = NULL;
            if (rev >= 0 && rev < DIKU_MAX_EXITS) {
                return_exit = target->exits[rev];
            }
            
            if (!return_exit || return_exit->to_room != room) {
                asymmetric++;
            }
        }
    }
    
    if (out_total) *out_total = total;
    if (out_asymmetric) *out_asymmetric = asymmetric;
    return asymmetric;
}

void diku_print_exit_symmetry(const area_t *area) {
    if (!area) return;
    
    int total = 0;
    int asymmetric = 0;
    diku_check_exit_symmetry(area, &total, &asymmetric);
    
    if (asymmetric == 0) {
        printf("All exits are symmetric (%d exits checked).\n", total);
        return;
    }
    
    printf("Exit symmetry report: %d of %d exits are asymmetric\n", asymmetric, total);
    printf("-------------------------------------------------------\n");
    
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
                printf("  #%d -> %s -> #%d missing return %s",
                       room->vnum, diku_dir_name(d), target->vnum, diku_dir_name(rev));
                if (return_exit && return_exit->to_room) {
                    printf(" (goes to #%d instead)\n", return_exit->to_room->vnum);
                } else if (return_exit) {
                    printf(" (unresolved)\n");
                } else {
                    printf("\n");
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Print functions for debugging                                      */
/* ------------------------------------------------------------------ */

void diku_print_room(const room_t *room) {
    if (!room) {
        printf("  (null room)\n");
        return;
    }
    
    printf("  Room #%d: %s\n", room->vnum, room->name.str ? room->name.str : "(unnamed)");
    printf("    Desc: %.60s%s\n", 
           room->desc.str ? room->desc.str : "(none)",
           room->desc.len > 60 ? "..." : "");
    printf("    Flags: 0x%x, Sector: %d\n", room->flags, room->sector);
    
    if (room->coord_assigned) {
        printf("    Coord: (%d, %d, %d)\n", room->coord.x, room->coord.y, room->coord.z);
    }
    
    printf("    Exits:");
    bool has_exits = false;
    for (int d = 0; d < DIKU_MAX_EXITS; d++) {
        if (room->exits[d] && room->exits[d]->to_vnum > 0) {
            printf(" %s->#%d", diku_dir_name_short(d), room->exits[d]->to_vnum);
            has_exits = true;
        }
    }
    if (!has_exits) printf(" (none)");
    printf("\n");
    
    if (room->extra_desc_count > 0) {
        printf("    Extra descs: %d\n", room->extra_desc_count);
    }
}

void diku_print_mobile(const mobile_t *mob) {
    if (!mob) {
        printf("  (null mobile)\n");
        return;
    }
    
    printf("  Mobile #%d: %s\n", mob->vnum,
           mob->short_desc.str ? mob->short_desc.str : "(unnamed)");
    printf("    Name: %s\n", mob->name.str ? mob->name.str : "(none)");
    printf("    Long: %s\n", mob->long_desc.str ? mob->long_desc.str : "(none)");
    printf("    Desc: %.60s%s\n",
           mob->description.str ? mob->description.str : "(none)",
           mob->description.len > 60 ? "..." : "");
    printf("    Level: %d, Align: %d, Sex: %d, Race: %d\n",
           mob->level, mob->alignment, mob->sex, mob->race);
    printf("    Hitroll: %d, Damroll: %d\n", mob->hitroll, mob->damroll);
    printf("    AC: %d/%d/%d/%d\n", mob->ac[0], mob->ac[1], mob->ac[2], mob->ac[3]);
    printf("    Gold: %ld, Silver: %ld\n", mob->gold, mob->silver);
    printf("    Size: %d, Form: %d, Parts: %d\n", mob->size, mob->form, mob->parts);
    
    if (mob->material.len > 0) {
        printf("    Material: %s\n", mob->material.str);
    }
}

void diku_print_item(const item_t *item) {
    if (!item) {
        printf("  (null item)\n");
        return;
    }
    
    printf("  Object #%d: %s\n", item->vnum,
           item->short_desc.str ? item->short_desc.str : "(unnamed)");
    printf("    Name: %s\n", item->name.str ? item->name.str : "(none)");
    printf("    Long: %s\n", item->long_desc.str ? item->long_desc.str : "(none)");
    printf("    Desc: %.60s%s\n",
           item->description.str ? item->description.str : "(none)",
           item->description.len > 60 ? "..." : "");
    printf("    Type: %s, Weight: %d, Cost: %d, Level: %d\n",
           diku_item_type_name(item->type), item->weight, item->cost, item->level);
    printf("    Values: %d %d %d %d %d\n",
           item->value[0], item->value[1], item->value[2], item->value[3], item->value[4]);
    printf("    Extra flags: 0x%x, Wear flags: 0x%x\n",
           item->extra_flags, item->wear_flags);
    
    if (item->material.len > 0) {
        printf("    Material: %s\n", item->material.str);
    }
    
    if (item->affect_count > 0) {
        printf("    Affects: %d\n", item->affect_count);
    }
    
    if (item->extra_desc_count > 0) {
        printf("    Extra descs: %d\n", item->extra_desc_count);
    }
}

void diku_print_area_info(const area_t *area) {
    if (!area) {
        printf("(null area)\n");
        return;
    }
    
    printf("Area: %s\n", area->name.str ? area->name.str : "(unnamed)");
    printf("  File: %s\n", area->filename.str ? area->filename.str : "(unknown)");
    printf("  Builders: %s\n", area->builders.str ? area->builders.str : "(none)");
    printf("  Rooms: %d\n", area->room_count);
    printf("  Mobiles: %d\n", area->mobile_count);
    printf("  Items: %d\n", area->item_count);
    
    if (area->low_vnum > 0 || area->high_vnum > 0) {
        printf("  Vnum range: %d - %d\n", area->low_vnum, area->high_vnum);
    }
}

void diku_print_graph(const area_t *area) {
    if (!area) return;
    
    printf("\nGraph for area: %s\n", area->name.str ? area->name.str : "(unnamed)");
    printf("================\n");
    
    for (int i = 0; i < area->room_count; i++) {
        diku_print_room(&area->rooms[i]);
    }
}

void diku_print_coordinates(area_t *area) {
    if (!area) return;
    
    printf("\n3D Coordinates for area: %s\n", area->name.str ? area->name.str : "(unnamed)");
    printf("========================\n");
    
    int min_x, max_x, min_y, max_y, min_z, max_z;
    diku_get_coord_bounds(area, &min_x, &max_x, &min_y, &max_y, &min_z, &max_z);
    
    printf("Bounds: X[%d,%d] Y[%d,%d] Z[%d,%d]\n", 
           min_x, max_x, min_y, max_y, min_z, max_z);
    
    /* Print layer by layer (z-slices) */
    for (int z = max_z; z >= min_z; z--) {
        bool has_rooms = false;
        for (int i = 0; i < area->room_count; i++) {
            if (area->rooms[i].coord_assigned && area->rooms[i].coord.z == z) {
                has_rooms = true;
                break;
            }
        }
        
        if (!has_rooms) continue;
        
        printf("\n--- Z = %d ---\n", z);
        
        /* Print grid */
        for (int y = max_y; y >= min_y; y--) {
            for (int x = min_x; x <= max_x; x++) {
                room_t *room = diku_room_at_coord(area, x, y, z);
                if (room) {
                    printf("[%4d]", room->vnum % 10000);
                } else {
                    printf("  ..  ");
                }
            }
            printf("\n");
        }
    }
}

/* diku_sector_name, diku_item_type_name — moved to static inline in diku/util.h */

diku_format_t diku_detect_format(const area_t *area) {
    if (!area) return DIKU_FMT_UNKNOWN;
    
    /* Heuristics based on content */
    
    /* Check for ROM-specific features */
    for (int i = 0; i < area->mobile_count; i++) {
        if (area->mobiles[i].material.len > 0) {
            return DIKU_FMT_ROM;
        }
    }
    for (int i = 0; i < area->item_count; i++) {
        if (area->items[i].material.len > 0 || area->items[i].level > 0) {
            return DIKU_FMT_ROM;
        }
    }
    
    /* Check for SMAUG features */
    if (area->security > 0 || area->version > 0) {
        return DIKU_FMT_SMAUG;
    }
    
    /* Default to Merc if we have reasonable data */
    if (area->room_count > 0 && area->mobile_count > 0) {
        return DIKU_FMT_MERC;
    }
    
    return DIKU_FMT_DIKU;
}

/* diku_format_name — moved to static inline in diku/util.h */

/* ------------------------------------------------------------------ */
/* Graph diameter calculation (for analysis)                          */
/* ------------------------------------------------------------------ */

static int bfs_distance(room_t *start, room_t *target, area_t *area) {
    if (start == target) return 0;
    if (!start || !target) return -1;
    
    /* Mark all unvisited */
    for (int i = 0; i < area->room_count; i++) {
        area->rooms[i].traversal_mark = -1;
    }
    
    room_t **queue = (room_t **)malloc(area->room_count * sizeof(room_t *));
    int qhead = 0, qtail = 0;
    
    start->traversal_mark = 0;
    queue[qtail++] = start;
    
    while (qhead < qtail) {
        room_t *current = queue[qhead++];
        int dist = current->traversal_mark;
        
        for (int d = 0; d < DIKU_MAX_EXITS; d++) {
            exit_t *exit = current->exits[d];
            if (!exit || !exit->to_room) continue;
            
            room_t *next = exit->to_room;
            if (next->traversal_mark >= 0) continue;
            
            next->traversal_mark = dist + 1;
            
            if (next == target) {
                free(queue);
                return dist + 1;
            }
            
            queue[qtail++] = next;
        }
    }
    
    free(queue);
    return -1;  /* Not reachable */
}

int diku_graph_diameter(area_t *area) {
    if (!area || area->room_count < 2) return 0;
    
    int diameter = 0;
    
    /* For each pair of rooms, find shortest path */
    for (int i = 0; i < area->room_count; i++) {
        for (int j = i + 1; j < area->room_count; j++) {
            int dist = bfs_distance(&area->rooms[i], &area->rooms[j], area);
            if (dist > diameter) {
                diameter = dist;
            }
        }
    }
    
    return diameter;
}
