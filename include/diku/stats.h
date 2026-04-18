#ifndef DIKU_STATS_H
#define DIKU_STATS_H

#include "diku/graph.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int areas;
    int rooms;
    int mobiles;
    int items;
    int exits;
    int resolved_exits;
    int sym_total;
    int sym_asymmetric;
} diku_totals_t;

void diku_compute_totals(area_t *areas, diku_totals_t *out);
void diku_print_totals(FILE *fp, const diku_totals_t *t, const char *source, area_t *areas);

#ifdef DIKU_PARSER_IMPLEMENTATION

void diku_compute_totals(area_t *areas, diku_totals_t *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    for (area_t *a = areas; a; a = a->next) {
        out->areas++;
        out->rooms += a->room_count;
        out->mobiles += a->mobile_count;
        out->items += a->item_count;
        for (int i = 0; i < a->room_count; i++) {
            for (int d = 0; d < DIKU_MAX_EXITS; d++) {
                exit_t *e = a->rooms[i].exits[d];
                if (e && e->to_vnum > 0) {
                    out->exits++;
                    if (e->to_room) out->resolved_exits++;
                }
            }
        }
        int t = 0, asym = 0;
        diku_check_exit_symmetry(a, &t, &asym);
        out->sym_total += t;
        out->sym_asymmetric += asym;
    }
}

void diku_print_totals(FILE *fp, const diku_totals_t *t, const char *source, area_t *areas) {
    if (!t || !fp) return;
    fprintf(fp, "============================================================\n");
    fprintf(fp, "Source: %s\n", source ? source : "");
    fprintf(fp, "Areas loaded: %d\n", t->areas);
    fprintf(fp, "============================================================\n");
    fprintf(fp, "Total Rooms:    %d\n", t->rooms);
    fprintf(fp, "Total Mobiles:  %d\n", t->mobiles);
    fprintf(fp, "Total Objects:  %d\n", t->items);
    fprintf(fp, "Total Exits:    %d (%.1f%% resolved)\n",
            t->exits, t->exits > 0 ? (100.0 * t->resolved_exits / t->exits) : 0.0);
    fprintf(fp, "Symmetry:       %d/%d asymmetric exits\n", t->sym_asymmetric, t->sym_total);
    fprintf(fp, "============================================================\n");
    for (area_t *a = areas; a; a = a->next) {
        fprintf(fp, "  %-30s  R:%4d  M:%4d  O:%4d\n",
                a->name.str && a->name.len > 0 ? a->name.str : "(unnamed)",
                a->room_count, a->mobile_count, a->item_count);
    }
    fprintf(fp, "============================================================\n");
}

#endif /* DIKU_PARSER_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif
#endif /* DIKU_STATS_H */
