#ifndef DIKU_COORDS_H
#define DIKU_COORDS_H

#include "diku/find.h"
#include "diku/context.h"
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

room_t *diku_find_central_room(area_t *area);
void diku_assign_coordinates(diku_context_t *ctx, area_t *area, room_t *root);
void diku_assign_coordinates_all(diku_context_t *ctx, area_t *areas);
void diku_assign_coordinates_multi(diku_context_t *ctx, area_t *areas);
room_t *diku_room_at_coord(area_t *area, int x, int y, int z);
bool diku_coord_occupied(area_t *area, int x, int y, int z, room_t *exclude);
void diku_center_coordinates(area_t *area);
void diku_get_coord_bounds(area_t *area, int *min_x, int *max_x,
                           int *min_y, int *max_y, int *min_z, int *max_z);

#ifdef DIKU_PARSER_IMPLEMENTATION

room_t *diku_find_central_room(area_t *area) {
    if (!area || area->room_count == 0) return NULL;
    if (area->room_count == 1) return &area->rooms[0];

    int *degree = (int *)calloc(area->room_count, sizeof(int));
    int *eccentricity = (int *)calloc(area->room_count, sizeof(int));
    for (int i = 0; i < area->room_count; i++) {
        for (int d = 0; d < DIKU_MAX_EXITS; d++) {
            if (area->rooms[i].exits[d] && area->rooms[i].exits[d]->to_vnum > 0) {
                degree[i]++;
            }
        }
    }
    int max_degree = 0;
    int max_degree_idx = 0;
    for (int i = 0; i < area->room_count; i++) {
        if (degree[i] > max_degree) {
            max_degree = degree[i];
            max_degree_idx = i;
        }
    }
    if (max_degree >= 4) {
        free(degree);
        free(eccentricity);
        return &area->rooms[max_degree_idx];
    }
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

/* ------------------------------------------------------------------ */
/* Coordinate hash (local to each assignment call)                    */
/* ------------------------------------------------------------------ */

typedef struct coord_entry {
    int x, y, z;
    room_t *room;
    struct coord_entry *next;
} coord_entry_t;

typedef struct {
    coord_entry_t **table;
    int size;
} diku_coord_hash_t;

static unsigned coord_hash_key(int x, int y, int z, int size) {
    unsigned h = ((unsigned)(x * 73856093) ^
                  (unsigned)(y * 19349663) ^
                  (unsigned)(z * 83492791));
    return h % (unsigned)size;
}

static bool coord_hash_check(diku_coord_hash_t *h, int x, int y, int z, room_t *exclude) {
    if (!h || !h->table) return false;
    unsigned key = coord_hash_key(x, y, z, h->size);
    coord_entry_t *e = h->table[key];
    while (e) {
        if (e->x == x && e->y == y && e->z == z) {
            if (e->room != exclude) return true;
        }
        e = e->next;
    }
    return false;
}

static void coord_hash_insert(diku_coord_hash_t *h, int x, int y, int z, room_t *room) {
    if (!h || !h->table) return;
    unsigned key = coord_hash_key(x, y, z, h->size);
    coord_entry_t *e = (coord_entry_t *)malloc(sizeof(coord_entry_t));
    e->x = x; e->y = y; e->z = z;
    e->room = room;
    e->next = h->table[key];
    h->table[key] = e;
}

static void coord_hash_free(diku_coord_hash_t *h) {
    if (!h || !h->table) return;
    for (int i = 0; i < h->size; i++) {
        coord_entry_t *e = h->table[i];
        while (e) {
            coord_entry_t *next = e->next;
            free(e);
            e = next;
        }
    }
    free(h->table);
    h->table = NULL;
    h->size = 0;
}

static void coord_hash_init(diku_coord_hash_t *h, int size) {
    h->size = size;
    h->table = (coord_entry_t **)calloc(size, sizeof(coord_entry_t *));
}

/* ------------------------------------------------------------------ */
/* Coordinate assignment                                              */
/* ------------------------------------------------------------------ */

static void diku_run_coord_bfs(diku_context_t *ctx, area_t *area, room_t *root,
                                int total_rooms, bool multi) {
    (void)multi;
    if (!area || !root || !ctx) return;

    for (int i = 0; i < area->room_count; i++) {
        area->rooms[i].coord_assigned = false;
        area->rooms[i].coord.x = area->rooms[i].coord.y = area->rooms[i].coord.z = 0;
    }

    diku_coord_hash_t ch = {0};
    coord_hash_init(&ch, total_rooms * 2 + 16);

    room_t **queue = (room_t **)malloc(total_rooms * sizeof(room_t *));
    int qhead = 0, qtail = 0;

    root->coord.x = root->coord.y = root->coord.z = 0;
    root->coord_assigned = true;
    coord_hash_insert(&ch, 0, 0, 0, root);
    queue[qtail++] = root;

    ctx->coord_bounds.min_x = ctx->coord_bounds.max_x = 0;
    ctx->coord_bounds.min_y = ctx->coord_bounds.max_y = 0;
    ctx->coord_bounds.min_z = ctx->coord_bounds.max_z = 0;
    ctx->coord_bounds.valid = true;

    while (qhead < qtail) {
        room_t *current = queue[qhead++];
        for (int d = 0; d < DIKU_MAX_EXITS; d++) {
            exit_t *exit = current->exits[d];
            if (!exit || !exit->to_room) continue;
            room_t *next = exit->to_room;
            if (next->coord_assigned) continue;
            int nx = current->coord.x + dir_offset[d][0];
            int ny = current->coord.y + dir_offset[d][1];
            int nz = current->coord.z + dir_offset[d][2];
            if (d != DIR_IN && d != DIR_OUT) {
                int attempts = 0;
                const int max_attempts = 26;
                while (coord_hash_check(&ch, nx, ny, nz, next) && attempts < max_attempts) {
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
                if (coord_hash_check(&ch, nx, ny, nz, next)) {
                    int radius = 2;
                    while (coord_hash_check(&ch, nx, ny, nz, next) && radius < 100) {
                        for (int dx = -radius; dx <= radius && coord_hash_check(&ch, nx, ny, nz, next); dx++) {
                            for (int dy = -radius; dy <= radius && coord_hash_check(&ch, nx, ny, nz, next); dy++) {
                                for (int dz = -radius; dz <= radius && coord_hash_check(&ch, nx, ny, nz, next); dz++) {
                                    int tx = current->coord.x + dir_offset[d][0] + dx;
                                    int ty = current->coord.y + dir_offset[d][1] + dy;
                                    int tz = current->coord.z + dir_offset[d][2] + dz;
                                    if (!coord_hash_check(&ch, tx, ty, tz, next)) {
                                        nx = tx; ny = ty; nz = tz;
                                    }
                                }
                            }
                        }
                        radius++;
                    }
                }
            }
            next->coord.x = nx;
            next->coord.y = ny;
            next->coord.z = nz;
            next->coord_assigned = true;
            coord_hash_insert(&ch, nx, ny, nz, next);
            if (nx < ctx->coord_bounds.min_x) ctx->coord_bounds.min_x = nx;
            if (nx > ctx->coord_bounds.max_x) ctx->coord_bounds.max_x = nx;
            if (ny < ctx->coord_bounds.min_y) ctx->coord_bounds.min_y = ny;
            if (ny > ctx->coord_bounds.max_y) ctx->coord_bounds.max_y = ny;
            if (nz < ctx->coord_bounds.min_z) ctx->coord_bounds.min_z = nz;
            if (nz > ctx->coord_bounds.max_z) ctx->coord_bounds.max_z = nz;
            queue[qtail++] = next;
        }
    }

    free(queue);
    coord_hash_free(&ch);
}

void diku_assign_coordinates(diku_context_t *ctx, area_t *area, room_t *root) {
    if (!ctx || !area || !root) return;
    diku_run_coord_bfs(ctx, area, root, area->room_count, false);
}

void diku_assign_coordinates_all(diku_context_t *ctx, area_t *areas) {
    if (!ctx || !areas) return;
    for (area_t *area = areas; area; area = area->next) {
        room_t *central = diku_find_central_room(area);
        if (central) {
            diku_assign_coordinates(ctx, area, central);
        }
    }
}

void diku_assign_coordinates_multi(diku_context_t *ctx, area_t *areas) {
    if (!ctx || !areas) return;
    int total_rooms = 0;
    for (area_t *area = areas; area; area = area->next) {
        total_rooms += area->room_count;
    }
    if (total_rooms == 0) return;
    for (area_t *area = areas; area; area = area->next) {
        for (int i = 0; i < area->room_count; i++) {
            area->rooms[i].coord_assigned = false;
            area->rooms[i].coord.x = area->rooms[i].coord.y = area->rooms[i].coord.z = 0;
        }
    }
    room_t *root = diku_find_central_room(areas);
    if (!root && areas->room_count > 0) root = &areas->rooms[0];
    if (!root) return;
    diku_run_coord_bfs(ctx, areas, root, total_rooms, true);
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

#endif /* DIKU_PARSER_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif
#endif /* DIKU_COORDS_H */
