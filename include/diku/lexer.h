#ifndef DIKU_LEXER_H
#define DIKU_LEXER_H

#include "diku/arena.h"

#ifdef __cplusplus
extern "C" {
#endif

bool diku_lexer_init_file(diku_lexer_t *lex, const char *filename);
bool diku_lexer_init_fp(diku_lexer_t *lex, FILE *fp);
void diku_lexer_init_buf(diku_lexer_t *lex, const char *buf, size_t len);
void diku_lexer_cleanup(diku_lexer_t *lex);

diku_token_t diku_lexer_next(diku_lexer_t *lex);
int diku_lexer_getc(diku_lexer_t *lex);
void diku_lexer_ungetc(diku_lexer_t *lex, int c);
long diku_lexer_tell(diku_lexer_t *lex);
void diku_lexer_seek(diku_lexer_t *lex, long pos);
int diku_lexer_peek(diku_lexer_t *lex);
void diku_lexer_skip_ws(diku_lexer_t *lex);
void diku_lexer_skip_line(diku_lexer_t *lex);
bool diku_lexer_read_word(diku_lexer_t *lex, char *out, size_t max);
bool diku_lexer_read_line(diku_lexer_t *lex, char *out, size_t max);
int diku_lexer_read_number(diku_lexer_t *lex, int *out);
diku_string_t diku_lexer_read_string(diku_lexer_t *lex, memento_arena_t *arena);
char *diku_lexer_read_word_dup(diku_lexer_t *lex, memento_arena_t *arena);
bool diku_lexer_eof(diku_lexer_t *lex);

#ifdef DIKU_PARSER_IMPLEMENTATION

bool diku_lexer_init_file(diku_lexer_t *lex, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return false;
    bool ok = diku_lexer_init_fp(lex, fp);
    fclose(fp);
    return ok;
}

bool diku_lexer_init_fp(diku_lexer_t *lex, FILE *fp) {
    if (!fp || fseek(fp, 0, SEEK_END) != 0) return false;
    long size = ftell(fp);
    if (size < 0) return false;
    rewind(fp);
    char *buf = (char *)malloc(size + 1);
    if (!buf) return false;
    size_t read = fread(buf, 1, size, fp);
    buf[read] = '\0';
    diku_lexer_init_buf(lex, buf, read);
    lex->owns_buf = true;
    return true;
}

void diku_lexer_init_buf(diku_lexer_t *lex, const char *buf, size_t len) {
    memset(lex, 0, sizeof(*lex));
    lex->buf = buf;
    lex->pos = buf;
    lex->end = buf + len;
    lex->line = 1;
    lex->col = 1;
    lex->owns_buf = false;
}

void diku_lexer_cleanup(diku_lexer_t *lex) {
    if (lex && lex->owns_buf) {
        free((void *)lex->buf);
        lex->buf = NULL;
        lex->pos = NULL;
        lex->end = NULL;
        lex->owns_buf = false;
    }
}

diku_token_t diku_lexer_next(diku_lexer_t *lex) {
    diku_token_t tok = {NULL, 0, TOK_EOF, lex->line, lex->col};

    while (lex->pos < lex->end) {
        char c = *lex->pos;
        if (c == '\n') { lex->line++; lex->col = 1; lex->pos++; }
        else if (c == '\r') { lex->pos++; }
        else if (c == '*') {
            while (lex->pos < lex->end && *lex->pos != '\n') lex->pos++;
            continue;
        }
        else if (isspace((unsigned char)c)) { lex->pos++; lex->col++; }
        else break;
    }

    if (lex->pos >= lex->end) return tok;

    const char *start = lex->pos;
    char c = *lex->pos++;
    int start_col = lex->col++;

    if (c == '#') {
        tok.type = TOK_HASH_SECTION;
        while (lex->pos < lex->end && !isspace((unsigned char)*lex->pos) && *lex->pos != '\n' && *lex->pos != '\r')
            lex->pos++;
        tok.start = start;
        tok.len = (size_t)(lex->pos - start);
        tok.line = lex->line;
        tok.col = start_col;
        return tok;
    }

    if (isdigit((unsigned char)c) || c == '-' || c == '+') {
        tok.type = TOK_NUMBER;
        while (lex->pos < lex->end && isdigit((unsigned char)*lex->pos)) { lex->pos++; lex->col++; }
        tok.start = start;
        tok.len = (size_t)(lex->pos - start);
        tok.line = lex->line;
        tok.col = start_col;
        return tok;
    }

    if (c == '~') {
        tok.type = TOK_STRING;
        tok.start = lex->pos;
        tok.len = 0;
        tok.line = lex->line;
        tok.col = start_col;
        return tok;
    }

    const char *tilde = memchr(lex->pos - 1, '~', (size_t)(lex->end - (lex->pos - 1)));
    if (tilde) {
        tok.type = TOK_STRING;
        tok.start = start + 1;
        tok.len = (size_t)(tilde - start - 1);
        while (tok.len > 0 && (tok.start[tok.len-1] == '\n' || tok.start[tok.len-1] == '\r' ||
                               tok.start[tok.len-1] == ' ' || tok.start[tok.len-1] == '\t'))
            tok.len--;
        lex->pos = tilde + 1;
        lex->col += (int)(lex->pos - start);
        tok.line = lex->line;
        tok.col = start_col;
        return tok;
    }

    tok.type = TOK_WORD;
    while (lex->pos < lex->end && !isspace((unsigned char)*lex->pos) && *lex->pos != '#' && *lex->pos != '~') {
        lex->pos++;
        lex->col++;
    }
    tok.start = start;
    tok.len = (size_t)(lex->pos - start);
    tok.line = lex->line;
    tok.col = start_col;
    return tok;
}

int diku_lexer_getc(diku_lexer_t *lex) {
    if (lex->pos >= lex->end) return EOF;
    char c = *lex->pos++;
    if (c == '\n') { lex->line++; lex->col = 1; }
    else { lex->col++; }
    return (unsigned char)c;
}

void diku_lexer_ungetc(diku_lexer_t *lex, int c) {
    (void)c;
    if (lex->pos <= lex->buf) return;
    lex->pos--;
    if (*lex->pos == '\n') {
        lex->line--;
        lex->col = 1;
        const char *p = lex->pos - 1;
        while (p >= lex->buf && *p != '\n') { lex->col++; p--; }
    } else {
        lex->col--;
    }
}

long diku_lexer_tell(diku_lexer_t *lex) {
    return (long)(lex->pos - lex->buf);
}

void diku_lexer_seek(diku_lexer_t *lex, long pos) {
    if (pos < 0) pos = 0;
    if (pos > (long)(lex->end - lex->buf)) pos = (long)(lex->end - lex->buf);
    lex->pos = lex->buf + pos;
    lex->line = 1;
    lex->col = 1;
    for (const char *p = lex->buf; p < lex->pos; p++) {
        if (*p == '\n') { lex->line++; lex->col = 1; }
        else if (*p != '\r') { lex->col++; }
    }
}

int diku_lexer_peek(diku_lexer_t *lex) {
    if (lex->pos >= lex->end) return EOF;
    return (unsigned char)*lex->pos;
}

void diku_lexer_skip_ws(diku_lexer_t *lex) {
    while (lex->pos < lex->end) {
        char c = *lex->pos;
        if (c == '\n') { lex->line++; lex->col = 1; lex->pos++; }
        else if (c == '\r') { lex->pos++; }
        else if (c == '*') {
            while (lex->pos < lex->end && *lex->pos != '\n') lex->pos++;
        }
        else if (isspace((unsigned char)c)) { lex->pos++; lex->col++; }
        else break;
    }
}

void diku_lexer_skip_line(diku_lexer_t *lex) {
    while (lex->pos < lex->end && *lex->pos != '\n') lex->pos++;
    if (lex->pos < lex->end && *lex->pos == '\n') { lex->pos++; lex->line++; lex->col = 1; }
}

bool diku_lexer_read_word(diku_lexer_t *lex, char *out, size_t max) {
    diku_lexer_skip_ws(lex);
    if (lex->pos >= lex->end) return false;
    size_t len = 0;
    while (lex->pos < lex->end && !isspace((unsigned char)*lex->pos)) {
        if (len + 1 < max) out[len] = *lex->pos;
        len++;
        lex->pos++;
        lex->col++;
    }
    if (len == 0) return false;
    out[len < max ? len : max - 1] = '\0';
    return true;
}

bool diku_lexer_read_line(diku_lexer_t *lex, char *out, size_t max) {
    if (lex->pos >= lex->end) return false;
    size_t len = 0;
    while (lex->pos < lex->end && *lex->pos != '\n' && *lex->pos != '\r') {
        if (len + 1 < max) out[len] = *lex->pos;
        len++;
        lex->pos++;
        lex->col++;
    }
    if (lex->pos < lex->end && (*lex->pos == '\n' || *lex->pos == '\r')) {
        if (*lex->pos == '\r') lex->pos++;
        if (lex->pos < lex->end && *lex->pos == '\n') lex->pos++;
        lex->line++;
        lex->col = 1;
    }
    out[len < max ? len : max - 1] = '\0';
    return true;
}

int diku_lexer_read_number(diku_lexer_t *lex, int *out) {
    diku_lexer_skip_ws(lex);
    if (lex->pos >= lex->end) return -1;

    const char *start = lex->pos;
    bool negative = false;
    if (*lex->pos == '-') { negative = true; lex->pos++; lex->col++; }
    else if (*lex->pos == '+') { lex->pos++; lex->col++; }

    if (lex->pos >= lex->end || !isdigit((unsigned char)*lex->pos)) {
        lex->pos = start;
        return -1;
    }

    long num = 0;
    while (lex->pos < lex->end && isdigit((unsigned char)*lex->pos)) {
        num = num * 10 + (*lex->pos - '0');
        lex->pos++;
        lex->col++;
    }

    *out = negative ? -(int)num : (int)num;
    return 0;
}

diku_string_t diku_lexer_read_string(diku_lexer_t *lex, memento_arena_t *arena) {
    diku_lexer_skip_ws(lex);
    diku_string_t result = {NULL, 0};
    if (lex->pos >= lex->end) return result;

    if (*lex->pos == '~') {
        lex->pos++;
        lex->col++;
        return result;
    }

    const char *start = lex->pos;
    const char *tilde = memchr(lex->pos, '~', (size_t)(lex->end - lex->pos));
    if (!tilde) tilde = lex->end;

    size_t len = (size_t)(tilde - start);
    while (len > 0 && (start[len-1] == '\n' || start[len-1] == '\r' || start[len-1] == ' ' || start[len-1] == '\t'))
        len--;

    result = diku_arena_strndup(arena, start, len);
    lex->pos = tilde + 1;
    lex->col += (int)(tilde - start + 1);
    return result;
}

char *diku_lexer_read_word_dup(diku_lexer_t *lex, memento_arena_t *arena) {
    char buf[256];
    if (!diku_lexer_read_word(lex, buf, sizeof(buf))) return NULL;
    return diku_arena_strdup(arena, buf);
}

bool diku_lexer_eof(diku_lexer_t *lex) {
    diku_lexer_skip_ws(lex);
    return lex->pos >= lex->end;
}

/* Static helpers used only by the section parsers (also in IMPLEMENTATION block) */
static char *diku_lexer_read_line_dup(diku_lexer_t *lex, memento_arena_t *arena) {
    char buf[4096];
    if (!diku_lexer_read_line(lex, buf, sizeof(buf))) return NULL;
    return diku_arena_strdup(arena, buf);
}

static diku_string_t diku_lexer_read_string_eol(diku_lexer_t *lex, memento_arena_t *arena) {
    diku_string_t result = {NULL, 0};
    char buf[4096];
    if (!diku_lexer_read_line(lex, buf, sizeof(buf))) return result;
    result.str = diku_arena_strdup(arena, buf);
    result.len = result.str ? strlen(result.str) : 0;
    return result;
}

static bool diku_lexer_read_letter(diku_lexer_t *lex, char expected) {
    diku_lexer_skip_ws(lex);
    if (lex->pos >= lex->end) return false;
    if (*lex->pos == expected) {
        lex->pos++;
        lex->col++;
        return true;
    }
    return false;
}

#endif /* DIKU_PARSER_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif
#endif /* DIKU_LEXER_H */
