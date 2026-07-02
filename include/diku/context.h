#ifndef DIKU_CONTEXT_H
#define DIKU_CONTEXT_H

#include "diku/arena.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Progress callback type                                             */
/* ------------------------------------------------------------------ */
/* (typedef lives in diku/types.h; this header uses it)               */

/* ------------------------------------------------------------------ */
/* Context — carries per-operation state (replaces diku_global)       */
/* Will be fully wired in a later migration step; declared here so    */
/* consumers can forward-declare / prepare.                           */
/* ------------------------------------------------------------------ */
typedef struct diku_context_t diku_context_t;

diku_context_t *diku_context_create(void);
void            diku_context_destroy(diku_context_t *ctx);
void            diku_context_set_progress(diku_context_t *ctx,
                                          diku_progress_cb_t cb, void *user);
void            diku_context_set_verbose(diku_context_t *ctx, bool verbose);

#ifdef DIKU_PARSER_IMPLEMENTATION

struct diku_context_t {
    memento_arena_t     *arena;
    diku_progress_cb_t   progress_cb;
    void                *progress_user;
    uint32_t             traversal_epoch;
    struct {
        int min_x, max_x, min_y, max_y, min_z, max_z;
        bool valid;
    } coord_bounds;
    uint32_t             flags;
    bool                 verbose;
};

diku_context_t *diku_context_create(void) {
    diku_context_t *ctx = (diku_context_t *)calloc(1, sizeof(diku_context_t));
    if (!ctx) return NULL;
    ctx->arena = diku_arena_create();
    return ctx;
}

void diku_context_destroy(diku_context_t *ctx) {
    if (!ctx) return;
    if (ctx->arena) diku_arena_free_all(ctx->arena);
    free(ctx);
}

void diku_context_set_progress(diku_context_t *ctx,
                               diku_progress_cb_t cb, void *user) {
    if (!ctx) return;
    ctx->progress_cb = cb;
    ctx->progress_user = user;
}

void diku_context_set_verbose(diku_context_t *ctx, bool verbose) {
    if (!ctx) return;
    ctx->verbose = verbose;
}

#endif /* DIKU_PARSER_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif
#endif /* DIKU_CONTEXT_H */
