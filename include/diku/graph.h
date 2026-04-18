#ifndef DIKU_GRAPH_H
#define DIKU_GRAPH_H

#include "diku/find.h"
#include "diku/context.h"

#ifdef __cplusplus
extern "C" {
#endif

void diku_resolve_graph(area_t **areas, int area_count);
void diku_resolve_graph_global(diku_context_t *ctx, area_t *areas);
int  diku_check_exit_symmetry(const area_t *area, int *out_total, int *out_asymmetric);
void diku_print_exit_symmetry(const area_t *area);
int  diku_graph_diameter(diku_context_t *ctx, area_t *area);

#ifdef DIKU_PARSER_IMPLEMENTATION

void diku_resolve_graph(area_t **areas, int area_count) {
    for (int a = 0; a < area_count; a++) {
        for (int r = 0; r < areas[a]->room_count; r++) {
            room_t *room = &areas[a]->rooms[r];
            for (int d = 0; d < DIKU_MAX_EXITS; d++) {
                exit_t *exit = room->exits[d];
                if (!exit) continue;
                exit->to_room = NULL;
                for (int aa = 0; aa < area_count; aa++) {
                    exit->to_room = diku_find_room(areas[aa], exit->to_vnum);
                    if (exit->to_room) break;
                }
            }
        }
    }
}

void diku_resolve_graph_global(diku_context_t *ctx, area_t *areas) {
    (void)ctx;
    if (!areas) return;
    int count = 0;
    for (area_t *a = areas; a; a = a->next) count++;
    area_t **area_array = (area_t **)malloc(count * sizeof(area_t *));
    if (!area_array) return;
    int i = 0;
    for (area_t *a = areas; a; a = a->next) area_array[i++] = a;
    diku_resolve_graph(area_array, count);
    free(area_array);
}

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
            if (rev >= 0 && rev < DIKU_MAX_EXITS) return_exit = target->exits[rev];
            if (!return_exit || return_exit->to_room != room) asymmetric++;
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
            if (rev >= 0 && rev < DIKU_MAX_EXITS) return_exit = target->exits[rev];
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

static bool bfs_room_in_area(const room_t *room, const area_t *area) {
    return room >= &area->rooms[0] && room < &area->rooms[area->room_count];
}

static int bfs_distance(diku_context_t *ctx, room_t *start, room_t *target, area_t *area) {
    if (start == target) return 0;
    if (!start || !target || !ctx || !area) return -1;
    uint32_t epoch = ++ctx->traversal_epoch;
    if (epoch == 0) epoch = ++ctx->traversal_epoch; /* avoid wrap to 0 */
    room_t **queue = (room_t **)malloc(area->room_count * sizeof(room_t *));
    int qhead = 0, qtail = 0;
    start->traversal_epoch = epoch;
    start->traversal_dist = 0;
    queue[qtail++] = start;
    while (qhead < qtail) {
        room_t *current = queue[qhead++];
        int dist = current->traversal_dist;
        for (int d = 0; d < DIKU_MAX_EXITS; d++) {
            exit_t *exit = current->exits[d];
            if (!exit || !exit->to_room) continue;
            room_t *next = exit->to_room;
            if (!bfs_room_in_area(next, area)) continue;
            if (next->traversal_epoch == epoch) continue;
            next->traversal_epoch = epoch;
            next->traversal_dist = dist + 1;
            if (next == target) {
                free(queue);
                return dist + 1;
            }
            queue[qtail++] = next;
        }
    }
    free(queue);
    return -1;
}

int diku_graph_diameter(diku_context_t *ctx, area_t *area) {
    if (!area || area->room_count < 2) return 0;
    int diameter = 0;
    for (int i = 0; i < area->room_count; i++) {
        for (int j = i + 1; j < area->room_count; j++) {
            int dist = bfs_distance(ctx, &area->rooms[i], &area->rooms[j], area);
            if (dist > diameter) diameter = dist;
        }
    }
    return diameter;
}

#endif /* DIKU_PARSER_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif
#endif /* DIKU_GRAPH_H */
