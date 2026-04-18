#ifndef DIKU_ARENA_H
#define DIKU_ARENA_H

#include "diku/types.h"

#ifdef __cplusplus
extern "C" {
#endif

memento_arena_t *diku_arena_create(void);
void *diku_arena_alloc(memento_arena_t *a, size_t n);
void *diku_arena_alloc_aligned(memento_arena_t *a, size_t n, size_t align);
char *diku_arena_strdup(memento_arena_t *a, const char *s);
diku_string_t diku_arena_strndup(memento_arena_t *a, const char *s, size_t len);
diku_string_t diku_arena_strdup_diku(memento_arena_t *a, const char *s);
void diku_arena_free_all(memento_arena_t *a);

#ifdef DIKU_PARSER_IMPLEMENTATION

memento_arena_t *diku_arena_create(void) {
    static bool memento_initialized = false;
    if (!memento_initialized) {
        memento_init();
        memento_initialized = true;
    }
    memento_thread_heap_t *heap = memento_thread_heap_get();
    return memento_arena_create(4 * 1024 * 1024, heap);
}

void *diku_arena_alloc(memento_arena_t *a, size_t n) {
    if (!a || n == 0) return NULL;
    return memento_arena_alloc(a, n, 8);
}

void *diku_arena_alloc_aligned(memento_arena_t *a, size_t n, size_t align) {
    if (!a || n == 0) return NULL;
    return memento_arena_alloc(a, n, align);
}

char *diku_arena_strdup(memento_arena_t *a, const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = (char *)diku_arena_alloc(a, len + 1);
    if (!copy) return NULL;
    memcpy(copy, s, len + 1);
    return copy;
}

diku_string_t diku_arena_strndup(memento_arena_t *a, const char *s, size_t len) {
    diku_string_t result = {NULL, 0};
    if (!s) return result;
    result.str = (char *)diku_arena_alloc(a, len + 1);
    if (!result.str) return result;
    memcpy(result.str, s, len);
    result.str[len] = '\0';
    result.len = len;
    return result;
}

diku_string_t diku_arena_strdup_diku(memento_arena_t *a, const char *s) {
    diku_string_t result = {NULL, 0};
    if (!s) return result;
    const char *end = strchr(s, '~');
    if (!end) end = s + strlen(s);
    size_t len = end - s;
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r' || s[len-1] == ' ' || s[len-1] == '\t'))
        len--;
    result.str = (char *)diku_arena_alloc(a, len + 1);
    if (!result.str) return result;
    memcpy(result.str, s, len);
    result.str[len] = '\0';
    result.len = len;
    return result;
}

void diku_arena_free_all(memento_arena_t *a) {
    if (a) memento_arena_destroy(a);
}

#endif /* DIKU_PARSER_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif
#endif /* DIKU_ARENA_H */
