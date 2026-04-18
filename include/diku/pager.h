#ifndef DIKU_PAGER_H
#define DIKU_PAGER_H

#include "diku/stats.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DIKU_PAGE_NEXT,
    DIKU_PAGE_QUIT,
    DIKU_PAGE_SKIP
} diku_page_action_t;

typedef void (*diku_pager_render_fn)(FILE *fp, const void *item, void *user);
typedef diku_page_action_t (*diku_pager_prompt_fn)(const char *category,
                                                    int cur, int total, void *user);

typedef struct diku_pager {
    const char            *category;
    FILE                  *out;
    diku_pager_render_fn   render;
    diku_pager_prompt_fn   prompt;
    void                  *user;
} diku_pager_t;

static inline diku_page_action_t diku_pager_default_prompt(const char *category,
                                                            int cur, int total,
                                                            void *user) {
    (void)user;
    printf("  [%s %d/%d]  [Enter: next | q: quit | n: next category] ",
           category, cur, total);
    fflush(stdout);
    char buf[16];
    if (!fgets(buf, sizeof(buf), stdin)) return DIKU_PAGE_QUIT;
    if (buf[0] == 'q' || buf[0] == 'Q') return DIKU_PAGE_QUIT;
    if (buf[0] == 'n' || buf[0] == 'N') return DIKU_PAGE_SKIP;
    return DIKU_PAGE_NEXT;
}

bool diku_pager_run_arealist_rooms(diku_pager_t *p, area_t *areas);
bool diku_pager_run_arealist_mobiles(diku_pager_t *p, area_t *areas);
bool diku_pager_run_arealist_items(diku_pager_t *p, area_t *areas);

#ifdef DIKU_PARSER_IMPLEMENTATION

static diku_page_action_t diku_pager_call_prompt(diku_pager_t *p, int cur, int total) {
    if (p->prompt) return p->prompt(p->category, cur, total, p->user);
    return diku_pager_default_prompt(p->category, cur, total, p->user);
}

bool diku_pager_run_arealist_rooms(diku_pager_t *p, area_t *areas) {
    if (!p || !p->render) return false;
    int total = 0;
    for (area_t *a = areas; a; a = a->next) total += a->room_count;
    if (total == 0) {
        fprintf(p->out ? p->out : stdout, "\n--- Rooms (0) ---\n");
        return false;
    }
    fprintf(p->out ? p->out : stdout, "\n--- Rooms (%d) ---\n", total);
    int idx = 0;
    for (area_t *a = areas; a; a = a->next) {
        for (int i = 0; i < a->room_count; i++) {
            fprintf(p->out ? p->out : stdout, "\n");
            p->render(p->out ? p->out : stdout, &a->rooms[i], p->user);
            diku_page_action_t action = diku_pager_call_prompt(p, ++idx, total);
            if (action == DIKU_PAGE_QUIT) return true;
            if (action == DIKU_PAGE_SKIP) return false;
        }
    }
    return false;
}

bool diku_pager_run_arealist_mobiles(diku_pager_t *p, area_t *areas) {
    if (!p || !p->render) return false;
    int total = 0;
    for (area_t *a = areas; a; a = a->next) total += a->mobile_count;
    if (total == 0) {
        fprintf(p->out ? p->out : stdout, "\n--- Mobiles (0) ---\n");
        return false;
    }
    fprintf(p->out ? p->out : stdout, "\n--- Mobiles (%d) ---\n", total);
    int idx = 0;
    for (area_t *a = areas; a; a = a->next) {
        for (int i = 0; i < a->mobile_count; i++) {
            fprintf(p->out ? p->out : stdout, "\n");
            p->render(p->out ? p->out : stdout, &a->mobiles[i], p->user);
            diku_page_action_t action = diku_pager_call_prompt(p, ++idx, total);
            if (action == DIKU_PAGE_QUIT) return true;
            if (action == DIKU_PAGE_SKIP) return false;
        }
    }
    return false;
}

bool diku_pager_run_arealist_items(diku_pager_t *p, area_t *areas) {
    if (!p || !p->render) return false;
    int total = 0;
    for (area_t *a = areas; a; a = a->next) total += a->item_count;
    if (total == 0) {
        fprintf(p->out ? p->out : stdout, "\n--- Objects (0) ---\n");
        return false;
    }
    fprintf(p->out ? p->out : stdout, "\n--- Objects (%d) ---\n", total);
    int idx = 0;
    for (area_t *a = areas; a; a = a->next) {
        for (int i = 0; i < a->item_count; i++) {
            fprintf(p->out ? p->out : stdout, "\n");
            p->render(p->out ? p->out : stdout, &a->items[i], p->user);
            diku_page_action_t action = diku_pager_call_prompt(p, ++idx, total);
            if (action == DIKU_PAGE_QUIT) return true;
            if (action == DIKU_PAGE_SKIP) return false;
        }
    }
    return false;
}

#endif /* DIKU_PARSER_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif
#endif /* DIKU_PAGER_H */
