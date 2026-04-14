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

/* ------------------------------------------------------------------ */
/* Arena allocator implementation                                     */
/* ------------------------------------------------------------------ */

arena_t *arena_create(void) {
    arena_t *a = (arena_t *)malloc(sizeof(arena_t));
    if (!a) return NULL;
    
    a->buf = (char *)malloc(DIKU_ARENA_BLOCK);
    if (!a->buf) {
        free(a);
        return NULL;
    }
    
    a->used = 0;
    a->cap = DIKU_ARENA_BLOCK;
    a->next = NULL;
    return a;
}

static void *arena_alloc_from(arena_t *a, size_t n, size_t align) {
    /* Calculate aligned offset */
    uintptr_t base = (uintptr_t)(a->buf + a->used);
    uintptr_t aligned = (base + align - 1) & ~(align - 1);
    size_t padding = aligned - base;
    
    if (a->used + padding + n > a->cap) {
        return NULL; /* Not enough space */
    }
    
    a->used += padding + n;
    return (void *)aligned;
}

void *arena_alloc(arena_t *a, size_t n) {
    /* Try current arena */
    void *p = arena_alloc_from(a, n, 8);
    if (p) return p;
    
    /* Need new arena block */
    arena_t *new_arena = arena_create();
    if (!new_arena) return NULL;
    
    /* Link and allocate from new */
    new_arena->next = a->next;
    a->next = new_arena;
    
    return arena_alloc_from(new_arena, n, 8);
}

void *arena_alloc_aligned(arena_t *a, size_t n, size_t align) {
    void *p = arena_alloc_from(a, n, align);
    if (p) return p;
    
    arena_t *new_arena = arena_create();
    if (!new_arena) return NULL;
    
    new_arena->next = a->next;
    a->next = new_arena;
    
    return arena_alloc_from(new_arena, n, align);
}

char *arena_strdup(arena_t *a, const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = (char *)arena_alloc(a, len + 1);
    if (!copy) return NULL;
    memcpy(copy, s, len + 1);
    return copy;
}

diku_string_t arena_strndup(arena_t *a, const char *s, size_t len) {
    diku_string_t result = {NULL, 0};
    if (!s) return result;
    
    result.str = (char *)arena_alloc(a, len + 1);
    if (!result.str) return result;
    
    memcpy(result.str, s, len);
    result.str[len] = '\0';
    result.len = len;
    return result;
}

diku_string_t arena_strdup_diku(arena_t *a, const char *s) {
    diku_string_t result = {NULL, 0};
    if (!s) return result;
    
    /* Diku strings end with ~ */
    const char *end = strchr(s, '~');
    if (!end) end = s + strlen(s);
    
    size_t len = end - s;
    
    /* Trim trailing whitespace/newlines */
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r' || s[len-1] == ' ' || s[len-1] == '\t'))
        len--;
    
    result.str = (char *)arena_alloc(a, len + 1);
    if (!result.str) return result;
    
    memcpy(result.str, s, len);
    result.str[len] = '\0';
    result.len = len;
    return result;
}

void arena_free_all(arena_t *a) {
    if (!a) return;
    
    /* Free all subsequent arenas first */
    if (a->next) {
        arena_free_all(a->next);
    }
    
    free(a->buf);
    free(a);
}

/* ------------------------------------------------------------------ */
/* Low-level file reading                                             */
/* ------------------------------------------------------------------ */

/* Skip whitespace and comments */
static void skip_whitespace(FILE *fp) {
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (isspace(c)) continue;
        if (c == '*') {  /* Comment - skip to end of line */
            while ((c = fgetc(fp)) != EOF && c != '\n');
            continue;
        }
        ungetc(c, fp);
        break;
    }
}

int diku_fread_number(FILE *fp, int *out) {
    skip_whitespace(fp);
    
    int c = fgetc(fp);
    if (c == EOF) return -1;
    
    /* Check for sign */
    bool negative = false;
    if (c == '-') {
        negative = true;
        c = fgetc(fp);
        if (c == EOF) return -1;
    } else if (c == '+') {
        c = fgetc(fp);
        if (c == EOF) return -1;
    }
    
    /* Must have at least one digit */
    if (!isdigit(c)) {
        ungetc(c, fp);
        return -1;
    }
    
    long num = 0;
    while (isdigit(c)) {
        num = num * 10 + (c - '0');
        c = fgetc(fp);
    }
    
    if (c != EOF) ungetc(c, fp);
    
    *out = negative ? -(int)num : (int)num;
    return 0;
}

/* Read a string terminated by ~ */
diku_string_t diku_fread_string(FILE *fp, arena_t *arena) {
    skip_whitespace(fp);
    
    /* Read until ~ */
    char buf[8192];
    size_t len = 0;
    int c;
    bool at_line_start = true;
    
    while ((c = fgetc(fp)) != EOF && c != '~' && len < sizeof(buf) - 1) {
        /* Safety: stop if we hit a line that looks like a section marker */
        if (at_line_start && c == '#') {
            long hash_pos = ftell(fp) - 1;
            int next = fgetc(fp);
            if (next == '0') {
                /* #0 - end of section */
                fseek(fp, hash_pos, SEEK_SET);
                break;
            } else if (isdigit(next)) {
                /* #<digits> - likely a new vnum/section */
                int pc = next;
                bool is_boundary = false;
                while (isdigit(pc)) {
                    pc = fgetc(fp);
                }
                if (pc == EOF || pc == '\n' || pc == '\r' || pc == ' ') {
                    is_boundary = true;
                }
                if (is_boundary) {
                    fseek(fp, hash_pos, SEEK_SET);
                    break;
                }
                /* Not a boundary - restore and continue */
                if (pc != EOF) ungetc(pc, fp);
                fseek(fp, hash_pos + 1, SEEK_SET);
            } else if (next != EOF) {
                ungetc(next, fp);
            }
        }
        buf[len++] = (char)c;
        at_line_start = (c == '\n');
    }
    buf[len] = '\0';
    
    /* Trim trailing whitespace */
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r' || buf[len-1] == ' ' || buf[len-1] == '\t'))
        len--;
    buf[len] = '\0';
    
    return arena_strndup(arena, buf, len);
}

/* Read string to end of line */
diku_string_t diku_fread_string_eol(FILE *fp, arena_t *arena) {
    diku_string_t result = {NULL, 0};
    
    char buf[4096];
    if (!fgets(buf, sizeof(buf), fp)) return result;
    
    size_t len = strlen(buf);
    /* Remove newline */
    if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
    if (len > 0 && buf[len-1] == '\r') buf[--len] = '\0';
    
    return arena_strndup(arena, buf, len);
}

/* Read a single word (whitespace delimited) */
char *diku_fread_word(FILE *fp, arena_t *arena) {
    skip_whitespace(fp);
    
    char buf[256];
    size_t len = 0;
    int c;
    
    while ((c = fgetc(fp)) != EOF && !isspace(c) && len < sizeof(buf) - 1) {
        buf[len++] = (char)c;
    }
    
    if (len == 0) return NULL;
    
    buf[len] = '\0';
    return arena_strdup(arena, buf);
}

/* Read rest of line */
char *diku_fread_line(FILE *fp, arena_t *arena) {
    char buf[4096];
    if (!fgets(buf, sizeof(buf), fp)) return NULL;
    
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
    if (len > 0 && buf[len-1] == '\r') buf[--len] = '\0';
    
    return arena_strdup(arena, buf);
}

/* Skip to end of current line */
void diku_fread_to_endline(FILE *fp) {
    int c;
    while ((c = fgetc(fp)) != EOF && c != '\n');
}

/* Read and expect a specific letter */
bool diku_fread_letter(FILE *fp, char expected) {
    skip_whitespace(fp);
    int c = fgetc(fp);
    if (c == EOF) return false;
    return (c == expected);
}

/* ------------------------------------------------------------------ */
/* Area header parser                                                 */
/* ------------------------------------------------------------------ */

static bool parse_header_line(area_t *area, FILE *fp, char letter, arena_t *arena) {
    switch (letter) {
        case 'K':  /* Builders */
            area->builders = diku_fread_string(fp, arena);
            return true;
            
        case 'L':  /* Level range - format: {low high}~ */
            diku_fread_to_endline(fp);  /* Skip for now, parse later if needed */
            return true;
            
        case 'N':  /* Security */
        case 'I':  /* Level range (alternative) */
        case 'V':  /* Vnum range */
        case 'X':  /* Version */
        case 'F':  /* Reset interval */
            diku_fread_to_endline(fp);
            return true;
            
        case 'U':  /* Ambient sound */
            area->ambient_sound = diku_fread_string(fp, arena);
            return true;
            
        case 'O':  /* Owner */
            area->owner = diku_fread_string(fp, arena);
            return true;
            
        case 'R':  /* Reset message */
            area->reset_msg = diku_fread_string(fp, arena);
            return true;
            
        case 'W':  /* Weather */
            area->weather = diku_fread_string(fp, arena);
            return true;
            
        case 'P':  /* Pay info */
            area->pay_info = diku_fread_string_eol(fp, arena);
            return true;
            
        case 'T':  /* Teleport info */
            area->teleport_info = diku_fread_string_eol(fp, arena);
            return true;
            
        case 'M':  /* Magic info */
            area->magic_info = diku_fread_string_eol(fp, arena);
            return true;
            
        default:
            /* Unknown header line - skip it */
            diku_fread_to_endline(fp);
            return true;
    }
}

area_t *diku_parse_area_header(FILE *fp, arena_t *arena) {
    area_t *area = (area_t *)arena_alloc_aligned(arena, sizeof(area_t), 64);
    if (!area) return NULL;
    
    memset(area, 0, sizeof(area_t));
    area->arena = arena;
    
    /* First line should be AREA or #AREA */
    char buf[256];
    if (!fgets(buf, sizeof(buf), fp)) return NULL;
    
    /* Check for AREA marker */
    if (strstr(buf, "AREA") || strstr(buf, "#AREA")) {
        /* Area name is on this line or next (terminated by ~) */
        char *name_start = strchr(buf, ' ');
        if (name_start) {
            name_start++;
            char *tilde = strchr(name_start, '~');
            if (tilde) {
                *tilde = '\0';
                /* Trim newline */
                size_t len = strlen(name_start);
                while (len > 0 && (name_start[len-1] == '\n' || name_start[len-1] == '\r'))
                    name_start[--len] = '\0';
                area->name = arena_strndup(arena, name_start, len);
            }
        }
        
        /* If no name found on this line, read from next line */
        if (!area->name.str || area->name.len == 0) {
            area->name = diku_fread_string(fp, arena);
        }
    }
    
    /* Parse header lines until we hit a section */
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (c == '*') {  /* Comment */
            diku_fread_to_endline(fp);
            continue;
        }
        
        if (c == '#') {  /* Section start - peek at next chars */
            long hash_pos = ftell(fp) - 1;  /* Position of # */
            char peek[16];
            if (fscanf(fp, "%15s", peek) == 1) {
                /* Check if it's a known section */
                if (strcmp(peek, "ROOMS") == 0 || strcmp(peek, "MOBILES") == 0 ||
                    strcmp(peek, "OBJECTS") == 0 || strcmp(peek, "HELPS") == 0 ||
                    strcmp(peek, "RESETS") == 0 || strcmp(peek, "SHOPS") == 0 ||
                    strcmp(peek, "SPECIALS") == 0 || strcmp(peek, "OBJFUNS") == 0 ||
                    strcmp(peek, "$") == 0 || isdigit(peek[0])) {
                    /* It's a section marker - seek back and break */
                    fseek(fp, hash_pos, SEEK_SET);
                    break;
                } else {
                    /* Not a section - treat as header line, skip rest of line */
                    diku_fread_to_endline(fp);
                    continue;
                }
            }
        }
        
        if (isspace(c)) continue;
        
        /* Header letter - but check if it's actually a section marker */
        /* Some files have unusual line endings that cause room content to be */
        /* misinterpreted as header lines. If we see a lowercase letter or a */
        /* character that's not a valid header letter, skip to end of line. */
        if (c < 'A' || c > 'Z') {
            /* Not a valid header letter - skip to end of line */
            diku_fread_to_endline(fp);
            continue;
        }
        
        /* Header letter */
        parse_header_line(area, fp, (char)c, arena);
    }
    
    return area;
}

/* ------------------------------------------------------------------ */
/* Room parser                                                        */
/* ------------------------------------------------------------------ */

static exit_t *parse_exit(FILE *fp, arena_t *arena, int direction) {
    exit_t *exit = (exit_t *)arena_alloc(arena, sizeof(exit_t));
    if (!exit) return NULL;
    
    memset(exit, 0, sizeof(exit_t));
    exit->direction = direction;
    exit->key_vnum = -1;
    exit->to_vnum = -1;
    
    /* Exit format:
     * D<dir>
     * <description>~
     * <keywords>~
     * <lock> <key_vnum> <to_vnum>
     */
    
    /* Description (can be empty) */
    exit->desc = diku_fread_string(fp, arena);
    
    /* Keywords */
    exit->keywords = diku_fread_string(fp, arena);
    
    /* Lock info line */
    skip_whitespace(fp);
    int lock, key, to_vnum;
    if (diku_fread_number(fp, &lock) == 0) {
        exit->flags = lock;
        diku_fread_number(fp, &key);
        exit->key_vnum = key;
        diku_fread_number(fp, &to_vnum);
        exit->to_vnum = to_vnum;
    }
    
    return exit;
}

room_t *diku_parse_room(FILE *fp, arena_t *arena, int *vnum_out) {
    int vnum;
    if (diku_fread_number(fp, &vnum) != 0) return NULL;
    
    *vnum_out = vnum;
    
    room_t *room = (room_t *)arena_alloc_aligned(arena, sizeof(room_t), 64);
    if (!room) return NULL;
    
    memset(room, 0, sizeof(room_t));
    room->vnum = vnum;
    room->coord_assigned = false;
    
    /* Name */
    room->name = diku_fread_string(fp, arena);
    
    /* Description */
    room->desc = diku_fread_string(fp, arena);
    
    /* Flags and sector - handle 2-field and 3-field formats */
    skip_whitespace(fp);
    long flags_pos = ftell(fp);
    int peek = fgetc(fp);
    if (peek != EOF) {
        if (peek == 'D' || peek == 'E' || peek == 'S' || peek == '~' || peek == '#' || peek == '*') {
            /* Missing flags/sector line - put it back */
            ungetc(peek, fp);
        } else {
            fseek(fp, flags_pos, SEEK_SET);
            char token1[64], token2[64];
            if (fscanf(fp, "%63s", token1) == 1) {
                long after_t1 = ftell(fp);
                /* Peek next non-space char */
                int c2;
                while ((c2 = fgetc(fp)) != EOF && isspace(c2)) {}
                if (c2 != EOF) {
                    ungetc(c2, fp);
                    if (isalpha((unsigned char)c2)) {
                        /* 3-field format: <num> <bitstring_flags> <sector> */
                        if (fscanf(fp, "%63s", token2) == 1) {
                            uint32_t flags = 0;
                            for (int i = 0; token2[i]; i++) {
                                if (token2[i] >= 'A' && token2[i] <= 'Z')
                                    flags |= (1U << (token2[i] - 'A'));
                                else if (token2[i] >= 'a' && token2[i] <= 'z')
                                    flags |= (1U << (26 + token2[i] - 'a'));
                            }
                            room->flags = flags;
                            diku_fread_number(fp, &room->sector);
                        }
                    } else {
                        /* 2-field format: <flags> <sector> */
                        fseek(fp, after_t1, SEEK_SET);
                        if (isdigit(token1[0]) || token1[0] == '-') {
                            room->flags = (uint32_t)strtol(token1, NULL, 0);
                        } else {
                            uint32_t flags = 0;
                            for (int i = 0; token1[i]; i++) {
                                if (token1[i] >= 'A' && token1[i] <= 'Z')
                                    flags |= (1U << (token1[i] - 'A'));
                                else if (token1[i] >= 'a' && token1[i] <= 'z')
                                    flags |= (1U << (26 + token1[i] - 'a'));
                            }
                            room->flags = flags;
                        }
                        diku_fread_number(fp, &room->sector);
                    }
                }
            }
        }
    }
    
    /* Parse room body until S */
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (c == '*') {  /* Comment */
            diku_fread_to_endline(fp);
            continue;
        }
        
        if (isspace(c)) continue;
        
        if (c == 'S' || c == 's') {  /* End of room - must be standalone */
            int next = fgetc(fp);
            if (next == '\n' || next == '\r' || next == EOF) {
                break;
            }
            if (next != EOF) ungetc(next, fp);
        }
        
        if (c == 'D' || c == 'd') {  /* Exit */
            int dir;
            if (diku_fread_number(fp, &dir) == 0 && dir >= 0 && dir < DIKU_MAX_EXITS) {
                room->exits[dir] = parse_exit(fp, arena, dir);
            } else {
                /* Invalid direction - skip this exit */
                diku_fread_string(fp, arena);  /* desc */
                diku_fread_string(fp, arena);  /* keywords */
                diku_fread_to_endline(fp);     /* lock line */
            }
            continue;
        }
        
        if (c == 'E') {  /* Extra description */
            /* Grow array */
            int idx = room->extra_desc_count++;
            size_t new_size = room->extra_desc_count * sizeof(*room->extra_descs);
            
            if (room->extra_desc_count == 1) {
                room->extra_descs = arena_alloc(arena, new_size);
            } else {
                /* Need to realloc - arena doesn't support this, so we chain */
                void *new_array = arena_alloc(arena, new_size);
                if (new_array && room->extra_descs) {
                    memcpy(new_array, room->extra_descs, (room->extra_desc_count - 1) * sizeof(*room->extra_descs));
                    room->extra_descs = new_array;
                }
            }
            
            if (room->extra_descs) {
                room->extra_descs[idx].keywords = diku_fread_string(fp, arena);
                room->extra_descs[idx].desc = diku_fread_string(fp, arena);
            }
            continue;
        }
        
        /* Unknown - skip to end of line */
        diku_fread_to_endline(fp);
    }
    
    return room;
}

/* ------------------------------------------------------------------ */
/* Mobile parser - handles Merc/ROM format variations                 */
/* ------------------------------------------------------------------ */

mobile_t *diku_parse_mobile(FILE *fp, arena_t *arena, int *vnum_out) {
    int vnum;
    if (diku_fread_number(fp, &vnum) != 0) return NULL;
    
    *vnum_out = vnum;
    
    mobile_t *mob = (mobile_t *)arena_alloc_aligned(arena, sizeof(mobile_t), 64);
    if (!mob) return NULL;
    
    memset(mob, 0, sizeof(mobile_t));
    mob->vnum = vnum;
    
    /* Name (keywords) */
    mob->name = diku_fread_string(fp, arena);
    
    /* Short description */
    mob->short_desc = diku_fread_string(fp, arena);
    
    /* Long description */
    mob->long_desc = diku_fread_string(fp, arena);
    
    /* Description (can be ~ for none) */
    diku_string_t desc = diku_fread_string(fp, arena);
    if (desc.len > 0 && strcmp(desc.str, "") != 0) {
        mob->description = desc;
    }
    
    /* Parse the numeric lines - format varies by fork */
    /* Merc/ROM format:
     * <act_flags> <aff_flags> <alignment> <letter=S for simple, more for complex>
     * <level> <hitroll> ...
     */
    
    skip_whitespace(fp);
    
    /* Read act flags (can be string like "abc" or number) */
    char buf[256];
    if (fscanf(fp, "%255s", buf) == 1) {
        /* Check if it's a number or bitstring */
        if (isdigit(buf[0]) || buf[0] == '-') {
            mob->act_flags = (uint32_t)strtol(buf, NULL, 0);
        } else {
            /* Bit string - decode */
            uint32_t flags = 0;
            for (int i = 0; buf[i]; i++) {
                if (buf[i] >= 'A' && buf[i] <= 'Z')
                    flags |= (1U << (buf[i] - 'A'));
                else if (buf[i] >= 'a' && buf[i] <= 'z')
                    flags |= (1U << (26 + buf[i] - 'a'));
            }
            mob->act_flags = flags;
        }
    }
    
    /* Affect flags */
    if (fscanf(fp, "%255s", buf) == 1) {
        if (isdigit(buf[0]) || buf[0] == '-') {
            mob->aff_flags = (uint32_t)strtol(buf, NULL, 0);
        } else {
            uint32_t flags = 0;
            for (int i = 0; buf[i]; i++) {
                if (buf[i] >= 'A' && buf[i] <= 'Z')
                    flags |= (1U << (buf[i] - 'A'));
                else if (buf[i] >= 'a' && buf[i] <= 'z')
                    flags |= (1U << (26 + buf[i] - 'a'));
            }
            mob->aff_flags = flags;
        }
    }
    
    /* Alignment */
    diku_fread_number(fp, (int *)&mob->alignment);
    
    /* Letter indicating format type */
    char format_letter = 'S';
    skip_whitespace(fp);
    int c = fgetc(fp);
    if (c != EOF && isalpha(c)) {
        format_letter = (char)c;
    } else if (c != EOF) {
        ungetc(c, fp);
    }
    
    /* Skip rest of this line */
    diku_fread_to_endline(fp);
    
    if (format_letter == 'S') {
        /* Simple format - one line of stats */
        /* level hitroll ac hp_dice mana_dice */
        diku_fread_number(fp, &mob->level);
        diku_fread_number(fp, &mob->hitroll);
        
        int ac, hp_num, hp_type, hp_bonus;
        diku_fread_number(fp, &ac);
        mob->ac[0] = mob->ac[1] = mob->ac[2] = mob->ac[3] = ac;
        
        diku_fread_number(fp, &hp_num);
        diku_fread_number(fp, &hp_type);
        diku_fread_number(fp, &hp_bonus);
        mob->hit[0] = hp_num;
        mob->hit[1] = hp_type;
        mob->hit[2] = hp_bonus;
        
        /* Skip mana/damage for simple format */
        diku_fread_to_endline(fp);
        
    } else if (format_letter == 'C' || format_letter == 'c') {
        /* Complex format (ROM) */
        /* level hitroll hp_dice mana_dice damage_dice */
        diku_fread_number(fp, &mob->level);
        diku_fread_number(fp, &mob->hitroll);
        
        diku_fread_number(fp, &mob->hit[0]);
        diku_fread_number(fp, &mob->hit[1]);
        diku_fread_number(fp, &mob->hit[2]);
        
        diku_fread_number(fp, &mob->mana[0]);
        diku_fread_number(fp, &mob->mana[1]);
        diku_fread_number(fp, &mob->mana[2]);
        
        diku_fread_number(fp, &mob->damage[0]);
        diku_fread_number(fp, &mob->damage[1]);
        diku_fread_number(fp, &mob->damage[2]);
        
        /* damage type */
        diku_fread_to_endline(fp);
        
        /* AC line */
        diku_fread_number(fp, &mob->ac[0]);  /* pierce */
        diku_fread_number(fp, &mob->ac[1]);  /* bash */
        diku_fread_number(fp, &mob->ac[2]);  /* slash */
        diku_fread_number(fp, &mob->ac[3]);  /* exotic */
        diku_fread_to_endline(fp);
        
        /* Offensive flags, immunities, resistances, vulnerabilities */
        skip_whitespace(fp);
        if (fscanf(fp, "%255s", buf) == 1) {
            mob->off_flags = (uint32_t)strtol(buf, NULL, 0);
        }
        if (fscanf(fp, "%255s", buf) == 1) {
            mob->imm_flags = (uint32_t)strtol(buf, NULL, 0);
        }
        if (fscanf(fp, "%255s", buf) == 1) {
            mob->res_flags = (uint32_t)strtol(buf, NULL, 0);
        }
        if (fscanf(fp, "%255s", buf) == 1) {
            mob->vuln_flags = (uint32_t)strtol(buf, NULL, 0);
        }
        diku_fread_to_endline(fp);
        
        /* Start pos, default pos, sex, wealth */
        diku_fread_number(fp, &mob->start_pos);
        diku_fread_number(fp, &mob->default_pos);
        diku_fread_number(fp, &mob->sex);
        
        skip_whitespace(fp);
        if (fscanf(fp, "%255s", buf) == 1) {
            /* Could be gold or race depending on fork */
            if (isdigit(buf[0])) {
                mob->gold = strtol(buf, NULL, 0);
            }
        }
        diku_fread_to_endline(fp);
        
    } else {
        /* Unknown format - store raw lines */
        /* Just skip for now */
        diku_fread_to_endline(fp);
    }
    
    /* Parse any extra lines (F lines for form, etc) */
    long pos = ftell(fp);
    int next_c;
    while ((next_c = fgetc(fp)) != EOF) {
        if (next_c == '#' || (next_c == '0' && (next_c = fgetc(fp)) == '\n')) {
            /* Next mobile or end of section */
            fseek(fp, pos, SEEK_SET);
            break;
        }
        if (next_c == 'F') {
            /* Form line - skip for now */
            diku_fread_to_endline(fp);
        } else if (next_c == 'M') {
            /* Material line */
            mob->material = diku_fread_string(fp, arena);
        } else if (next_c != '\n' && next_c != '\r') {
            /* Unknown - skip */
            diku_fread_to_endline(fp);
        }
        pos = ftell(fp);
    }
    
    return mob;
}

/* ------------------------------------------------------------------ */
/* Item/Object parser                                                 */
/* ------------------------------------------------------------------ */

item_t *diku_parse_item(FILE *fp, arena_t *arena, int *vnum_out) {
    long start_pos = ftell(fp);
    int vnum;
    if (diku_fread_number(fp, &vnum) != 0) {
        return NULL;
    }
    
    *vnum_out = vnum;
    
    item_t *item = (item_t *)arena_alloc_aligned(arena, sizeof(item_t), 64);
    if (!item) return NULL;
    
    memset(item, 0, sizeof(item_t));
    item->vnum = vnum;
    
    /* Name (keywords) */
    item->name = diku_fread_string(fp, arena);
    
    /* Short description */
    item->short_desc = diku_fread_string(fp, arena);
    
    /* Long description */
    item->long_desc = diku_fread_string(fp, arena);
    
    /* Description (can be ~ for none, or omitted entirely in some forks) */
    skip_whitespace(fp);
    int desc_peek = fgetc(fp);
    if (desc_peek != EOF) {
        if (desc_peek == '~') {
            /* Empty description */
        } else if (isdigit(desc_peek) || desc_peek == '#' || desc_peek == '0') {
            /* No description - next field is numeric data or section marker */
            ungetc(desc_peek, fp);
        } else {
            /* Has a description string */
            ungetc(desc_peek, fp);
            diku_string_t desc = diku_fread_string(fp, arena);
            if (desc.len > 0 && strcmp(desc.str, "") != 0) {
                item->description = desc;
            }
        }
    }
    
    /* Safety: if we hit a section boundary, the item is missing numeric data */
    skip_whitespace(fp);
    long boundary_pos = ftell(fp);
    int boundary_c = fgetc(fp);
    if (boundary_c == '#') {
        int boundary_next = fgetc(fp);
        if (boundary_next == '0') {
            /* End of section */
            fseek(fp, boundary_pos, SEEK_SET);
            return item;
        } else if (boundary_next != EOF) {
            ungetc(boundary_next, fp);
        }
    }
    if (boundary_c != EOF) {
        ungetc(boundary_c, fp);
    }
    
    /* Type, extra flags, wear flags */
    skip_whitespace(fp);
    diku_fread_number(fp, &item->type);
    
    /* Flags can be numeric or bitstring */
    char buf[256];
    if (fscanf(fp, "%255s", buf) == 1) {
        if (isdigit(buf[0]) || buf[0] == '-') {
            item->extra_flags = (uint32_t)strtol(buf, NULL, 0);
        } else {
            uint32_t flags = 0;
            for (int i = 0; buf[i]; i++) {
                if (buf[i] >= 'A' && buf[i] <= 'Z')
                    flags |= (1U << (buf[i] - 'A'));
                else if (buf[i] >= 'a' && buf[i] <= 'z')
                    flags |= (1U << (26 + buf[i] - 'a'));
            }
            item->extra_flags = flags;
        }
    }
    
    if (fscanf(fp, "%255s", buf) == 1) {
        if (isdigit(buf[0]) || buf[0] == '-') {
            item->wear_flags = (uint32_t)strtol(buf, NULL, 0);
        } else {
            uint32_t flags = 0;
            for (int i = 0; buf[i]; i++) {
                if (buf[i] >= 'A' && buf[i] <= 'Z')
                    flags |= (1U << (buf[i] - 'A'));
                else if (buf[i] >= 'a' && buf[i] <= 'z')
                    flags |= (1U << (26 + buf[i] - 'a'));
            }
            item->wear_flags = flags;
        }
    }
    diku_fread_to_endline(fp);
    
    /* Values (4 or 5 depending on fork) */
    skip_whitespace(fp);
    for (int i = 0; i < 4; i++) {
        diku_fread_number(fp, &item->value[i]);
    }
    diku_fread_to_endline(fp);
    
    /* Weight, cost, level (optional) */
    skip_whitespace(fp);
    diku_fread_number(fp, &item->weight);
    diku_fread_number(fp, &item->cost);
    
    /* Try to read level - may not exist */
    long pos = ftell(fp);
    int level;
    if (diku_fread_number(fp, &level) == 0) {
        item->level = level;
    } else {
        fseek(fp, pos, SEEK_SET);
    }
    diku_fread_to_endline(fp);
    
    /* Parse extra lines (A for affects, E for extra descs, L for level, etc) */
    pos = ftell(fp);
    int next_c;
    while ((next_c = fgetc(fp)) != EOF) {
        if (next_c == '#' || (next_c == '0' && (next_c = fgetc(fp)) == '\n')) {
            fseek(fp, pos, SEEK_SET);
            break;
        }
        
        if (next_c == 'A') {  /* Affect */
            int idx = item->affect_count++;
            size_t new_size = item->affect_count * sizeof(*item->affects);
            
            void *new_array = arena_alloc(arena, new_size);
            if (new_array && item->affects) {
                memcpy(new_array, item->affects, (item->affect_count - 1) * sizeof(*item->affects));
                item->affects = new_array;
            } else {
                item->affects = new_array;
            }
            
            if (item->affects) {
                diku_fread_number(fp, &item->affects[idx].location);
                diku_fread_number(fp, &item->affects[idx].modifier);
            }
            diku_fread_to_endline(fp);
            
        } else if (next_c == 'E') {  /* Extra description */
            int idx = item->extra_desc_count++;
            size_t new_size = item->extra_desc_count * sizeof(*item->extra_descs);
            
            void *new_array = arena_alloc(arena, new_size);
            if (new_array && item->extra_descs) {
                memcpy(new_array, item->extra_descs, (item->extra_desc_count - 1) * sizeof(*item->extra_descs));
                item->extra_descs = new_array;
            } else {
                item->extra_descs = new_array;
            }
            
            if (item->extra_descs) {
                item->extra_descs[idx].keywords = diku_fread_string(fp, arena);
                item->extra_descs[idx].desc = diku_fread_string(fp, arena);
            }
            
        } else if (next_c == 'L') {  /* Level (alternative) */
            diku_fread_number(fp, &item->level);
            diku_fread_to_endline(fp);
            
        } else if (next_c == 'M') {  /* Material */
            item->material = diku_fread_string(fp, arena);
            
        } else if (next_c != '\n' && next_c != '\r') {
            diku_fread_to_endline(fp);
        }
        
        pos = ftell(fp);
    }
    
    return item;
}

/* ------------------------------------------------------------------ */
/* Classic multi-file section parsers (CircleMUD / original DikuMUD)  */
/* ------------------------------------------------------------------ */

bool diku_parse_wld_fp(FILE *fp, area_t *area) {
    if (!fp || !area) return false;
    
    int room_capacity = 16;
    if (area->room_count > 0) {
        room_capacity = area->room_count * 2;
        room_t *new_rooms = (room_t *)arena_alloc_aligned(area->arena, sizeof(room_t) * room_capacity, 64);
        if (new_rooms && area->rooms) {
            memcpy(new_rooms, area->rooms, area->room_count * sizeof(room_t));
            area->rooms = new_rooms;
        }
    } else {
        area->rooms = (room_t *)arena_alloc_aligned(area->arena, sizeof(room_t) * room_capacity, 64);
    }
    
    int parsed = 0;
    while (1) {
        skip_whitespace(fp);
        int c = fgetc(fp);
        if (c == EOF) break;
        if (c == '$') break;
        if (c == '#') {
            int next = fgetc(fp);
            if (next == '$' || next == '0') {
                break;
            }
            if (isdigit(next)) {
                ungetc(next, fp);
                int vnum;
                room_t *room = diku_parse_room(fp, area->arena, &vnum);
                if (!room) break;
                
                if (area->room_count >= room_capacity) {
                    room_capacity *= 2;
                    room_t *new_rooms = (room_t *)arena_alloc_aligned(area->arena, sizeof(room_t) * room_capacity, 64);
                    if (new_rooms && area->rooms) {
                        memcpy(new_rooms, area->rooms, area->room_count * sizeof(room_t));
                        area->rooms = new_rooms;
                    }
                }
                
                if (area->rooms) {
                    memcpy(&area->rooms[area->room_count++], room, sizeof(room_t));
                }
                parsed++;
                continue;
            }
            if (next != EOF) ungetc(next, fp);
        }
        if (c != EOF) ungetc(c, fp);
        diku_fread_to_endline(fp);
    }
    
    return parsed > 0;
}

bool diku_parse_mob_fp(FILE *fp, area_t *area) {
    if (!fp || !area) return false;
    
    int mob_capacity = 16;
    if (area->mobile_count > 0) {
        mob_capacity = area->mobile_count * 2;
        mobile_t *new_mobs = (mobile_t *)arena_alloc_aligned(area->arena, sizeof(mobile_t) * mob_capacity, 64);
        if (new_mobs && area->mobiles) {
            memcpy(new_mobs, area->mobiles, area->mobile_count * sizeof(mobile_t));
            area->mobiles = new_mobs;
        }
    } else {
        area->mobiles = (mobile_t *)arena_alloc_aligned(area->arena, sizeof(mobile_t) * mob_capacity, 64);
    }
    
    int parsed = 0;
    while (1) {
        skip_whitespace(fp);
        int c = fgetc(fp);
        if (c == EOF) break;
        if (c == '$') break;
        if (c == '#') {
            int next = fgetc(fp);
            if (next == '$' || next == '0') {
                break;
            }
            if (isdigit(next)) {
                ungetc(next, fp);
                int vnum;
                mobile_t *mob = diku_parse_mobile(fp, area->arena, &vnum);
                if (!mob) break;
                
                if (area->mobile_count >= mob_capacity) {
                    mob_capacity *= 2;
                    mobile_t *new_mobs = (mobile_t *)arena_alloc_aligned(area->arena, sizeof(mobile_t) * mob_capacity, 64);
                    if (new_mobs && area->mobiles) {
                        memcpy(new_mobs, area->mobiles, area->mobile_count * sizeof(mobile_t));
                        area->mobiles = new_mobs;
                    }
                }
                
                if (area->mobiles) {
                    memcpy(&area->mobiles[area->mobile_count++], mob, sizeof(mobile_t));
                }
                parsed++;
                continue;
            }
            if (next != EOF) ungetc(next, fp);
        }
        if (c != EOF) ungetc(c, fp);
        diku_fread_to_endline(fp);
    }
    
    return parsed > 0;
}

bool diku_parse_obj_fp(FILE *fp, area_t *area) {
    if (!fp || !area) return false;
    
    int item_capacity = 16;
    if (area->item_count > 0) {
        item_capacity = area->item_count * 2;
        item_t *new_items = (item_t *)arena_alloc_aligned(area->arena, sizeof(item_t) * item_capacity, 64);
        if (new_items && area->items) {
            memcpy(new_items, area->items, area->item_count * sizeof(item_t));
            area->items = new_items;
        }
    } else {
        area->items = (item_t *)arena_alloc_aligned(area->arena, sizeof(item_t) * item_capacity, 64);
    }
    
    int parsed = 0;
    while (1) {
        skip_whitespace(fp);
        int c = fgetc(fp);
        if (c == EOF) break;
        if (c == '$') break;
        if (c == '#') {
            int next = fgetc(fp);
            if (next == '$' || next == '0') {
                break;
            }
            if (isdigit(next)) {
                ungetc(next, fp);
                int vnum;
                item_t *item = diku_parse_item(fp, area->arena, &vnum);
                if (!item) break;
                
                if (area->item_count >= item_capacity) {
                    item_capacity *= 2;
                    item_t *new_items = (item_t *)arena_alloc_aligned(area->arena, sizeof(item_t) * item_capacity, 64);
                    if (new_items && area->items) {
                        memcpy(new_items, area->items, area->item_count * sizeof(item_t));
                        area->items = new_items;
                    }
                }
                
                if (area->items) {
                    memcpy(&area->items[area->item_count++], item, sizeof(item_t));
                }
                parsed++;
                continue;
            }
            if (next != EOF) ungetc(next, fp);
        }
        if (c != EOF) ungetc(c, fp);
        diku_fread_to_endline(fp);
    }
    
    return parsed > 0;
}

bool diku_parse_zon_fp(FILE *fp, area_t *area) {
    if (!fp || !area) return false;
    
    skip_whitespace(fp);
    
    /* Optional zone number header: #<num> */
    int c = fgetc(fp);
    if (c == '#') {
        int zone_num;
        diku_fread_number(fp, &zone_num);
        skip_whitespace(fp);
        c = fgetc(fp);
    }
    if (c != EOF) ungetc(c, fp);
    
    /* Zone name */
    diku_string_t name = diku_fread_string(fp, area->arena);
    if (name.len > 0 && (!area->name.str || area->name.len == 0)) {
        area->name = name;
    }
    
    /* Header numbers: top_vnum lifespan reset_mode ... */
    diku_fread_to_endline(fp);
    
    /* Read reset commands until S or $ */
    char **lines = NULL;
    int count = 0;
    char buf[4096];
    
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
            buf[--len] = '\0';
        }
        
        if (len == 0) continue;
        if (buf[0] == '*') continue;
        if (buf[0] == 'S' || buf[0] == 's' || buf[0] == '$') {
            break;
        }
        
        count++;
        char **new_lines = (char **)arena_alloc(area->arena, count * sizeof(char *));
        if (new_lines && lines) {
            memcpy(new_lines, lines, (count - 1) * sizeof(char *));
        }
        lines = new_lines;
        if (lines) {
            lines[count - 1] = arena_strdup(area->arena, buf);
        }
    }
    
    area->resets_raw = lines;
    area->resets_line_count = count;
    diku_parse_resets(area);
    
    return true;
}

area_t *diku_parse_package_files(const char *wld, const char *mob, const char *obj, const char *zon) {
    arena_t *arena = arena_create();
    if (!arena) return NULL;
    
    area_t *area = (area_t *)arena_alloc_aligned(arena, sizeof(area_t), 64);
    if (!area) {
        arena_free_all(arena);
        return NULL;
    }
    memset(area, 0, sizeof(area_t));
    area->arena = arena;
    
    bool has_data = false;
    FILE *fp;
    
    fp = fopen(wld, "r");
    if (fp) {
        area->filename = arena_strndup(arena, wld, strlen(wld));
        if (diku_parse_wld_fp(fp, area)) has_data = true;
        fclose(fp);
    }
    
    fp = fopen(mob, "r");
    if (fp) {
        if (diku_parse_mob_fp(fp, area)) has_data = true;
        fclose(fp);
    }
    
    fp = fopen(obj, "r");
    if (fp) {
        if (diku_parse_obj_fp(fp, area)) has_data = true;
        fclose(fp);
    }
    
    fp = fopen(zon, "r");
    if (fp) {
        if (diku_parse_zon_fp(fp, area)) has_data = true;
        fclose(fp);
    }
    
    if (!has_data) {
        diku_free_area(area);
        return NULL;
    }
    
    /* Build vnum hash table */
    if (area->room_count > 0) {
        area->rooms_by_vnum = (room_t **)calloc(DIKU_VNUM_HASH_SIZE, sizeof(room_t *));
        for (int i = 0; i < area->room_count; i++) {
            int hash = area->rooms[i].vnum & DIKU_VNUM_HASH_MASK;
            if (!area->rooms_by_vnum[hash]) {
                area->rooms_by_vnum[hash] = &area->rooms[i];
            }
        }
    }
    
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

static char **store_raw_section(FILE *fp, arena_t *arena, int *line_count) {
    char **lines = NULL;
    int count = 0;
    
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) {
        /* Check for end of section */
        if (buf[0] == '0' && (buf[1] == '\n' || buf[1] == '\r' || buf[1] == '\0')) {
            break;
        }
        if (buf[0] == '#') {
            /* Next section */
            fseek(fp, -(long)strlen(buf), SEEK_CUR);
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
        
        lines[count - 1] = arena_strdup(arena, buf);
    }
    
    *line_count = count;
    return lines;
}

/* ------------------------------------------------------------------ */
/* Main file parser                                                   */
/* ------------------------------------------------------------------ */

area_t *diku_parse_fp(FILE *fp, const char *filename) {
    if (!fp) return NULL;
    
    arena_t *arena = arena_create();
    if (!arena) return NULL;
    
    area_t *area = diku_parse_area_header(fp, arena);
    if (!area) {
        arena_free_all(arena);
        return NULL;
    }
    
    area->filename = arena_strndup(arena, filename, strlen(filename));
    
    /* Parse sections */
    char buf[256];
    while (fscanf(fp, "%255s", buf) == 1) {
        if (buf[0] != '#') {
            /* Not a section marker */
            continue;
        }
        
        const char *section = buf + 1;
        
        if (strcmp(section, "ROOMS") == 0) {
            /* Parse rooms */
            int room_capacity = 16;
            area->rooms = (room_t *)arena_alloc_aligned(arena, sizeof(room_t) * room_capacity, 64);
            if (!area->rooms) {
                fprintf(stderr, "ERROR: Failed to allocate rooms array\n");
                continue;
            }
            
            int vnum;
            while (1) {
                skip_whitespace(fp);
                long pos = ftell(fp);
                
                /* Check for end of rooms */
                int c = fgetc(fp);
                if (c == '#') {
                    char next[16];
                    if (fscanf(fp, "%15s", next) == 1) {
                        if (strcmp(next, "0") == 0) {
                            break;  /* End of rooms */
                        } else {
                            /* It's a room vnum - skip the # and position after it */
                            fseek(fp, pos + 1, SEEK_SET);
                        }
                    }
                } else if (c == '0') {
                    break;  /* End of rooms */
                } else if (c == EOF) {
                    break;
                } else {
                    fseek(fp, pos, SEEK_SET);
                    diku_fread_to_endline(fp);
                    continue;
                }
                
                /* Parse room */
                room_t *room = diku_parse_room(fp, arena, &vnum);
                if (!room) {
                    fprintf(stderr, "diku_parse_room NULL at pos %ld\n", ftell(fp));
                    break;
                }
                fprintf(stderr, "ROOM #%d %s\n", vnum, room->name.str ? room->name.str : "");
                
                /* Grow array if needed */
                if (area->room_count >= room_capacity) {
                    room_capacity *= 2;
                    room_t *new_rooms = (room_t *)arena_alloc_aligned(arena, sizeof(room_t) * room_capacity, 64);
                    memcpy(new_rooms, area->rooms, area->room_count * sizeof(room_t));
                    area->rooms = new_rooms;
                }
                
                memcpy(&area->rooms[area->room_count++], room, sizeof(room_t));
            }
            
        } else if (strcmp(section, "MOBILES") == 0 || strcmp(section, "MOBILE") == 0) {
            /* Parse mobiles */
            int mob_capacity = 16;
            area->mobiles = (mobile_t *)arena_alloc_aligned(arena, sizeof(mobile_t) * mob_capacity, 64);
            
            int vnum;
            while (1) {
                skip_whitespace(fp);
                long pos = ftell(fp);
                
                int c = fgetc(fp);
                if (c == '#') {
                    char next[16];
                    if (fscanf(fp, "%15s", next) == 1) {
                        if (strcmp(next, "0") == 0) {
                            break;
                        } else {
                            fseek(fp, pos + 1, SEEK_SET);
                        }
                    }
                } else if (c == '0') {
                    break;
                } else if (c == EOF) {
                    break;
                } else {
                    fseek(fp, pos, SEEK_SET);
                    diku_fread_to_endline(fp);
                    continue;
                }
                
                mobile_t *mob = diku_parse_mobile(fp, arena, &vnum);
                if (!mob) break;
                
                if (area->mobile_count >= mob_capacity) {
                    mob_capacity *= 2;
                    mobile_t *new_mobs = (mobile_t *)arena_alloc_aligned(arena, sizeof(mobile_t) * mob_capacity, 64);
                    memcpy(new_mobs, area->mobiles, area->mobile_count * sizeof(mobile_t));
                    area->mobiles = new_mobs;
                }
                
                memcpy(&area->mobiles[area->mobile_count++], mob, sizeof(mobile_t));
            }
            
        } else if (strcmp(section, "OBJECTS") == 0 || strcmp(section, "OBJECT") == 0 || 
                   strcmp(section, "OBJ") == 0) {

            /* Parse items */
            int item_capacity = 16;
            area->items = (item_t *)arena_alloc_aligned(arena, sizeof(item_t) * item_capacity, 64);
            
            int vnum;
            while (1) {
                skip_whitespace(fp);
                long pos = ftell(fp);
                
                int c = fgetc(fp);
                if (c == '#') {
                    char next[16];
                    if (fscanf(fp, "%15s", next) == 1) {
                        if (strcmp(next, "0") == 0) {
                            break;
                        } else {
                            fseek(fp, pos + 1, SEEK_SET);
                        }
                    }
                } else if (c == '0') {
                    break;
                } else if (c == EOF) {
                    break;
                } else {
                    fseek(fp, pos, SEEK_SET);
                    diku_fread_to_endline(fp);
                    continue;
                }
                
                item_t *item = diku_parse_item(fp, arena, &vnum);

                if (!item) {
                    break;
                }
                
                if (area->item_count >= item_capacity) {
                    item_capacity *= 2;
                    item_t *new_items = (item_t *)arena_alloc_aligned(arena, sizeof(item_t) * item_capacity, 64);
                    memcpy(new_items, area->items, area->item_count * sizeof(item_t));
                    area->items = new_items;
                }
                
                memcpy(&area->items[area->item_count++], item, sizeof(item_t));
            }
            
        } else if (strcmp(section, "HELPS") == 0) {
            area->helps_raw = store_raw_section(fp, arena, &area->helps_line_count);
            
        } else if (strcmp(section, "RESETS") == 0 || strcmp(section, "RESET") == 0) {
            area->resets_raw = store_raw_section(fp, arena, &area->resets_line_count);
            
        } else if (strcmp(section, "SHOPS") == 0 || strcmp(section, "SHOP") == 0) {
            area->shops_raw = store_raw_section(fp, arena, &area->shops_line_count);
            
        } else if (strcmp(section, "SPECIALS") == 0 || strcmp(section, "SPECIAL") == 0) {
            area->specials_raw = store_raw_section(fp, arena, &area->specials_line_count);
            
        } else if (strcmp(section, "OBJFUNS") == 0 || strcmp(section, "OBJFUN") == 0) {
            area->objfuns_raw = store_raw_section(fp, arena, &area->objfuns_line_count);
            
        } else if (strcmp(section, "$") == 0) {
            /* End of file */
            break;
            
        } else {
            /* Unknown section - store as extra */
            area->extra_section_count++;
            /* Use malloc for this since it's dynamic */
            /* For now, just skip the section */
            store_raw_section(fp, arena, &(int){0});
        }
    }
    
    /* Build vnum hash table for fast room lookup */
    if (area->room_count > 0) {
        area->rooms_by_vnum = (room_t **)calloc(DIKU_VNUM_HASH_SIZE, sizeof(room_t *));
        for (int i = 0; i < area->room_count; i++) {
            int hash = area->rooms[i].vnum & DIKU_VNUM_HASH_MASK;
            /* Simple chaining - store first room at hash, link others */
            if (!area->rooms_by_vnum[hash]) {
                area->rooms_by_vnum[hash] = &area->rooms[i];
            }
        }
    }
    
    /* Parse resets into room contents */
    diku_parse_resets(area);
    
    return area;
}

/* ------------------------------------------------------------------ */
/* Reset parser - populate room contents from #RESETS                 */
/* ------------------------------------------------------------------ */

void diku_parse_resets(area_t *area) {
    if (!area || !area->resets_raw || area->resets_line_count <= 0) return;
    
    mobile_t *last_mob = NULL;
    
    for (int i = 0; i < area->resets_line_count; i++) {
        char *line = area->resets_raw[i];
        if (!line || !line[0]) continue;
        
        char cmd;
        int arg1, arg2, arg3, arg4, arg5;
        int n = sscanf(line, " %c %d %d %d %d %d", &cmd, &arg1, &arg2, &arg3, &arg4, &arg5);
        if (n < 2) continue;
        
        switch (cmd) {
            case 'M':
            case 'm': {
                /* M 0 <mob_vnum> <limit> <room_vnum> */
                if (n < 5) break;
                int mob_vnum = arg2;
                int room_vnum = arg4;
                mobile_t *mob = diku_find_mobile(area, mob_vnum);
                room_t *room = diku_find_room(area, room_vnum);
                if (mob && room) {
                    /* Add mob to room */
                    int idx = room->room_mobile_count++;
                    size_t new_size = room->room_mobile_count * sizeof(mobile_t *);
                    mobile_t **new_array = (mobile_t **)arena_alloc(area->arena, new_size);
                    if (new_array && room->room_mobiles) {
                        memcpy(new_array, room->room_mobiles, idx * sizeof(mobile_t *));
                    }
                    room->room_mobiles = new_array;
                    if (room->room_mobiles) {
                        room->room_mobiles[idx] = mob;
                    }
                    last_mob = mob;
                }
                break;
            }
            
            case 'O':
            case 'o': {
                /* O 0 <obj_vnum> <limit> <room_vnum> */
                if (n < 5) break;
                int obj_vnum = arg2;
                int room_vnum = arg4;
                item_t *item = diku_find_item(area, obj_vnum);
                room_t *room = diku_find_room(area, room_vnum);
                if (item && room) {
                    int idx = room->room_item_count++;
                    size_t new_size = room->room_item_count * sizeof(item_t *);
                    item_t **new_array = (item_t **)arena_alloc(area->arena, new_size);
                    if (new_array && room->room_items) {
                        memcpy(new_array, room->room_items, idx * sizeof(item_t *));
                    }
                    room->room_items = new_array;
                    if (room->room_items) {
                        room->room_items[idx] = item;
                    }
                }
                break;
            }
            
            case 'G':
            case 'g': {
                /* G 0 <obj_vnum> <limit> - give to last mob */
                if (n < 4 || !last_mob) break;
                int obj_vnum = arg2;
                item_t *item = diku_find_item(area, obj_vnum);
                if (item && last_mob) {
                    int idx = last_mob->inventory_count++;
                    size_t new_size = last_mob->inventory_count * sizeof(*last_mob->inventory);
                    void *new_array = arena_alloc(area->arena, new_size);
                    if (new_array && last_mob->inventory) {
                        memcpy(new_array, last_mob->inventory, idx * sizeof(*last_mob->inventory));
                    }
                    last_mob->inventory = new_array;
                    if (last_mob->inventory) {
                        last_mob->inventory[idx].item = item;
                        last_mob->inventory[idx].wear_loc = -1;
                    }
                }
                break;
            }
            
            case 'E':
            case 'e': {
                /* E 0 <obj_vnum> <limit> <wear_loc> - equip on last mob */
                if (n < 5 || !last_mob) break;
                int obj_vnum = arg2;
                int wear_loc = arg4;
                item_t *item = diku_find_item(area, obj_vnum);
                if (item && last_mob) {
                    int idx = last_mob->inventory_count++;
                    size_t new_size = last_mob->inventory_count * sizeof(*last_mob->inventory);
                    void *new_array = arena_alloc(area->arena, new_size);
                    if (new_array && last_mob->inventory) {
                        memcpy(new_array, last_mob->inventory, idx * sizeof(*last_mob->inventory));
                    }
                    last_mob->inventory = new_array;
                    if (last_mob->inventory) {
                        last_mob->inventory[idx].item = item;
                        last_mob->inventory[idx].wear_loc = wear_loc;
                    }
                }
                break;
            }
            
            case 'P':
            case 'p':
            case 'D':
            case 'd':
            case 'R':
            case 'r':
            case 'S':
            case 's':
            default:
                break;
        }
    }
}

area_t *diku_parse_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp) {
        area_t *area = diku_parse_fp(fp, filename);
        fclose(fp);
        return area;
    }
    
    /* If not a regular file, try as a multi-file package base path */
    return diku_parse_package(filename);
}

/* ------------------------------------------------------------------ */
/* Graph resolution - link exits to rooms                             */
/* ------------------------------------------------------------------ */

room_t *diku_find_room(area_t *area, int vnum) {
    if (!area) return NULL;
    
    /* Linear search through rooms */
    for (int i = 0; i < area->room_count; i++) {
        if (area->rooms[i].vnum == vnum) {
            return &area->rooms[i];
        }
    }
    
    return NULL;
}

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

/* ------------------------------------------------------------------ */
/* Lookup functions                                                   */
/* ------------------------------------------------------------------ */

mobile_t *diku_find_mobile(area_t *area, int vnum) {
    if (!area) return NULL;
    for (int i = 0; i < area->mobile_count; i++) {
        if (area->mobiles[i].vnum == vnum) {
            return &area->mobiles[i];
        }
    }
    return NULL;
}

item_t *diku_find_item(area_t *area, int vnum) {
    if (!area) return NULL;
    for (int i = 0; i < area->item_count; i++) {
        if (area->items[i].vnum == vnum) {
            return &area->items[i];
        }
    }
    return NULL;
}

room_t *diku_find_room_global(area_t *areas, int vnum) {
    for (area_t *area = areas; area; area = area->next) {
        room_t *room = diku_find_room(area, vnum);
        if (room) return room;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Cleanup functions                                                  */
/* ------------------------------------------------------------------ */

void diku_free_area(area_t *area) {
    if (!area) return;
    
    /* Save pointer to hash table (not in arena) */
    void *hash_table = area->rooms_by_vnum;
    arena_t *arena = area->arena;
    
    /* Free the hash table first */
    if (hash_table) {
        free(hash_table);
    }
    
    /* Free the arena - this frees everything else including the area struct */
    if (arena) {
        arena_free_all(arena);
    }
}

void diku_free_all_areas(area_t *areas) {
    while (areas) {
        area_t *next = areas->next;
        diku_free_area(areas);
        areas = next;
    }
}

/* ------------------------------------------------------------------ */
/* Utility functions                                                  */
/* ------------------------------------------------------------------ */

const char *diku_dir_name(int dir) {
    static const char *names[] = {
        "north", "east", "south", "west", "up", "down",
        "northeast", "northwest", "southeast", "southwest",
        "in", "out"
    };
    if (dir < 0 || dir >= DIKU_MAX_EXITS) return "unknown";
    return names[dir];
}

const char *diku_dir_name_short(int dir) {
    static const char *names[] = {
        "n", "e", "s", "w", "u", "d",
        "ne", "nw", "se", "sw", "in", "out"
    };
    if (dir < 0 || dir >= DIKU_MAX_EXITS) return "?";
    return names[dir];
}

int diku_reverse_dir(int dir) {
    static const int reverse[] = {
        DIR_SOUTH, DIR_WEST, DIR_NORTH, DIR_EAST, DIR_DOWN, DIR_UP,
        DIR_SOUTHWEST, DIR_SOUTHEAST, DIR_NORTHWEST, DIR_NORTHEAST,
        DIR_OUT, DIR_IN
    };
    if (dir < 0 || dir >= DIKU_MAX_EXITS) return -1;
    return reverse[dir];
}

bool diku_has_exit(const room_t *room, int dir) {
    if (!room || dir < 0 || dir >= DIKU_MAX_EXITS) return false;
    return room->exits[dir] != NULL && room->exits[dir]->to_vnum > 0;
}

int diku_get_exit_vnum(const room_t *room, int dir) {
    if (!diku_has_exit(room, dir)) return -1;
    return room->exits[dir]->to_vnum;
}

int diku_count_exits(const room_t *room) {
    if (!room) return 0;
    int count = 0;
    for (int i = 0; i < DIKU_MAX_EXITS; i++) {
        if (room->exits[i] && room->exits[i]->to_vnum > 0) {
            count++;
        }
    }
    return count;
}

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

/* ------------------------------------------------------------------ */
/* Format detection and names                                         */
/* ------------------------------------------------------------------ */

const char *diku_sector_name(int sector) {
    static const char *names[] = {
        "inside", "city", "field", "forest", "hills", "mountain",
        "water_swim", "water_noswim", "underwater", "air", "desert",
        "unknown", "unknown", "unknown", "unknown", "unknown"
    };
    if (sector < 0 || sector >= 16) return "unknown";
    return names[sector];
}

const char *diku_item_type_name(int type) {
    static const char *names[] = {
        "none", "light", "scroll", "wand", "staff", "weapon",
        "unused", "unused", "treasure", "armor", "potion",
        "unused", "furniture", "trash", "unused", "container",
        "unused", "drink", "key", "food", "money", "unused",
        "boat", "corpse_npc", "corpse_pc", "fountain", "pill",
        "protect", "map", "portal", "warp_stone", "room_key",
        "gem", "jewelry", "jitbag"
    };
    if (type < 0 || type >= 35) return "unknown";
    return names[type];
}

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

const char *diku_format_name(diku_format_t fmt) {
    switch (fmt) {
        case DIKU_FMT_DIKU:    return "DikuMUD";
        case DIKU_FMT_MERC:    return "Merc";
        case DIKU_FMT_ROM:     return "ROM";
        case DIKU_FMT_CIRCLE:  return "CircleMUD";
        case DIKU_FMT_SMAUG:   return "SMAUG";
        case DIKU_FMT_CUSTOM:  return "Custom";
        default:               return "Unknown";
    }
}

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
