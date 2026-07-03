#ifndef DIKU_SCHEMA_H
#define DIKU_SCHEMA_H

#include "diku/types.h"
#include <strings.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Fork detection                                                     */
/* ------------------------------------------------------------------ */

diku_format_t diku_sniff_format(const char *buf, size_t len);
diku_format_t diku_classify_area(const area_t *area);
int           diku_format_priority(diku_format_t fmt);

/* ------------------------------------------------------------------ */
/* Bitvector decoding (ROM/SMAUG alphabetic bitvectors)               */
/* A=1<<0, B=1<<1, ... Z=1<<25, aa=1<<26, bb=1<<27, ...               */
/* ------------------------------------------------------------------ */
uint32_t diku_decode_bitvector(const char *s);

/* ------------------------------------------------------------------ */
/* Canonical sector types                                             */
/* ------------------------------------------------------------------ */
enum {
    DIKU_SECT_INSIDE        = 0,
    DIKU_SECT_CITY          = 1,
    DIKU_SECT_FIELD         = 2,
    DIKU_SECT_FOREST        = 3,
    DIKU_SECT_HILLS         = 4,
    DIKU_SECT_MOUNTAIN      = 5,
    DIKU_SECT_WATER_SWIM    = 6,
    DIKU_SECT_WATER_NOSWIM  = 7,
    DIKU_SECT_UNDERWATER    = 8,
    DIKU_SECT_AIR           = 9,
    DIKU_SECT_DESERT        = 10
};

/* ------------------------------------------------------------------ */
/* Canonical item types                                               */
/* ------------------------------------------------------------------ */
enum {
    DIKU_ITEM_NONE          = 0,
    DIKU_ITEM_LIGHT         = 1,
    DIKU_ITEM_SCROLL        = 2,
    DIKU_ITEM_WAND          = 3,
    DIKU_ITEM_STAFF         = 4,
    DIKU_ITEM_WEAPON        = 5,
    DIKU_ITEM_TREASURE      = 8,
    DIKU_ITEM_ARMOR         = 9,
    DIKU_ITEM_POTION        = 10,
    DIKU_ITEM_FURNITURE     = 12,
    DIKU_ITEM_TRASH         = 13,
    DIKU_ITEM_CONTAINER     = 15,
    DIKU_ITEM_DRINK_CON     = 17,
    DIKU_ITEM_KEY           = 18,
    DIKU_ITEM_FOOD          = 19,
    DIKU_ITEM_MONEY         = 20,
    DIKU_ITEM_BOAT          = 22,
    DIKU_ITEM_CORPSE_NPC    = 23,
    DIKU_ITEM_CORPSE_PC     = 24,
    DIKU_ITEM_FOUNTAIN      = 25,
    DIKU_ITEM_PILL          = 26
};

/* ------------------------------------------------------------------ */
/* Canonical room flags                                               */
/* ------------------------------------------------------------------ */
enum {
    DIKU_ROOM_DARK          = (1U << 0),
    DIKU_ROOM_NO_MOB        = (1U << 1),
    DIKU_ROOM_INDOORS       = (1U << 2),
    DIKU_ROOM_PEACEFUL      = (1U << 3),
    DIKU_ROOM_STEAL         = (1U << 4),
    DIKU_ROOM_NO_MAGIC      = (1U << 5),
    DIKU_ROOM_TUNNEL        = (1U << 6),
    DIKU_ROOM_PRIVATE       = (1U << 7),
    DIKU_ROOM_GODS_ONLY     = (1U << 8),
    DIKU_ROOM_NO_RECALL     = (1U << 9),
    DIKU_ROOM_IMP_ONLY      = (1U << 10),
    DIKU_ROOM_GOD_ONLY      = (1U << 11),
    DIKU_ROOM_HEROES_ONLY   = (1U << 12),
    DIKU_ROOM_NEWBIES_ONLY  = (1U << 13),
    DIKU_ROOM_LAW           = (1U << 14),
    DIKU_ROOM_NOWHERE       = (1U << 15),
    DIKU_ROOM_SILENCE       = (1U << 20),
    DIKU_ROOM_LOGSPEECH     = (1U << 21),
    DIKU_ROOM_NOCAMP        = (1U << 22),
    DIKU_ROOM_NOSUMMON      = (1U << 23),
    DIKU_ROOM_NOASTRAL      = (1U << 24)
};

/* ------------------------------------------------------------------ */
/* Canonical mobile act flags                                         */
/* ------------------------------------------------------------------ */
enum {
    DIKU_ACT_IS_NPC             = (1U << 0),
    DIKU_ACT_SENTINEL           = (1U << 1),
    DIKU_ACT_SCAVENGER          = (1U << 2),
    DIKU_ACT_AGGRESSIVE         = (1U << 3),
    DIKU_ACT_STAY_AREA          = (1U << 4),
    DIKU_ACT_WIMPY              = (1U << 5),
    DIKU_ACT_PET                = (1U << 6),
    DIKU_ACT_TRAIN              = (1U << 7),
    DIKU_ACT_PRACTICE           = (1U << 8),
    DIKU_ACT_UNDEAD             = (1U << 9),
    DIKU_ACT_CLERIC             = (1U << 10),
    DIKU_ACT_MAGE               = (1U << 11),
    DIKU_ACT_THIEF              = (1U << 12),
    DIKU_ACT_WARRIOR            = (1U << 13),
    DIKU_ACT_NOALIGN            = (1U << 14),
    DIKU_ACT_NOPURGE            = (1U << 15),
    DIKU_ACT_OUTDOORS           = (1U << 16),
    DIKU_ACT_INDOORS            = (1U << 17),
    DIKU_ACT_IS_HEALER          = (1U << 18),
    DIKU_ACT_GAIN               = (1U << 19),
    DIKU_ACT_UPDATE_ALWAYS      = (1U << 20),
    DIKU_ACT_IS_CHANGER         = (1U << 21),
    DIKU_ACT_AGGRESSIVE_EVIL    = (1U << 22),
    DIKU_ACT_AGGRESSIVE_GOOD    = (1U << 23),
    DIKU_ACT_AGGRESSIVE_NEUTRAL = (1U << 24),
    DIKU_ACT_WERE               = (1U << 25),
    DIKU_ACT_MOUNT              = (1U << 26)
};

/* ------------------------------------------------------------------ */
/* Canonical affect flags                                             */
/* ------------------------------------------------------------------ */
enum {
    DIKU_AFF_BLIND          = (1U << 0),
    DIKU_AFF_INVISIBLE      = (1U << 1),
    DIKU_AFF_DETECT_EVIL    = (1U << 2),
    DIKU_AFF_DETECT_INVIS   = (1U << 3),
    DIKU_AFF_DETECT_MAGIC   = (1U << 4),
    DIKU_AFF_DETECT_HIDDEN  = (1U << 5),
    DIKU_AFF_HOLD           = (1U << 6),
    DIKU_AFF_SANCTUARY      = (1U << 7),
    DIKU_AFF_FAERIE_FIRE    = (1U << 8),
    DIKU_AFF_INFRARED       = (1U << 9),
    DIKU_AFF_CURSE          = (1U << 10),
    DIKU_AFF_NO_FLAG        = (1U << 11),
    DIKU_AFF_POISON         = (1U << 12),
    DIKU_AFF_PROTECT_EVIL   = (1U << 13),
    DIKU_AFF_PROTECT_GOOD   = (1U << 14),
    DIKU_AFF_SNEAK          = (1U << 15),
    DIKU_AFF_HIDE           = (1U << 16),
    DIKU_AFF_SLEEP          = (1U << 17),
    DIKU_AFF_CHARM          = (1U << 18),
    DIKU_AFF_FLYING         = (1U << 19),
    DIKU_AFF_PASS_DOOR      = (1U << 20),
    DIKU_AFF_HASTE          = (1U << 21),
    DIKU_AFF_CALM           = (1U << 22),
    DIKU_AFF_PLAGUE         = (1U << 23),
    DIKU_AFF_WEAKEN         = (1U << 24),
    DIKU_AFF_DARK_VISION    = (1U << 25),
    DIKU_AFF_BERSERK        = (1U << 26),
    DIKU_AFF_SWIM           = (1U << 27),
    DIKU_AFF_REGENERATION   = (1U << 28),
    DIKU_AFF_SLOW           = (1U << 29)
};

/* ------------------------------------------------------------------ */
/* Canonical object wear flags                                        */
/* ------------------------------------------------------------------ */
enum {
    DIKU_WEAR_TAKE          = (1U << 0),
    DIKU_WEAR_FINGER        = (1U << 1),
    DIKU_WEAR_NECK          = (1U << 2),
    DIKU_WEAR_BODY          = (1U << 3),
    DIKU_WEAR_HEAD          = (1U << 4),
    DIKU_WEAR_LEGS          = (1U << 5),
    DIKU_WEAR_FEET          = (1U << 6),
    DIKU_WEAR_HANDS         = (1U << 7),
    DIKU_WEAR_ARMS          = (1U << 8),
    DIKU_WEAR_SHIELD        = (1U << 9),
    DIKU_WEAR_ABOUT         = (1U << 10),
    DIKU_WEAR_WAIST         = (1U << 11),
    DIKU_WEAR_WRIST         = (1U << 12),
    DIKU_WEAR_WIELD         = (1U << 13),
    DIKU_WEAR_HOLD          = (1U << 14),
    DIKU_WEAR_TWO_HANDS     = (1U << 15),
    DIKU_WEAR_EARS          = (1U << 16),
    DIKU_WEAR_EYES          = (1U << 17),
    DIKU_WEAR_LIGHT         = (1U << 18),
    DIKU_WEAR_BACK          = (1U << 19),
    DIKU_WEAR_FACE          = (1U << 20),
    DIKU_WEAR_ANKLE         = (1U << 21)
};

/* ------------------------------------------------------------------ */
/* Canonical object extra flags                                       */
/* ------------------------------------------------------------------ */
enum {
    DIKU_EXTRA_GLOW             = (1U << 0),
    DIKU_EXTRA_HUM              = (1U << 1),
    DIKU_EXTRA_DARK             = (1U << 2),
    DIKU_EXTRA_LOCK             = (1U << 3),
    DIKU_EXTRA_EVIL             = (1U << 4),
    DIKU_EXTRA_INVIS            = (1U << 5),
    DIKU_EXTRA_MAGIC            = (1U << 6),
    DIKU_EXTRA_NODROP           = (1U << 7),
    DIKU_EXTRA_BLESS            = (1U << 8),
    DIKU_EXTRA_ANTI_GOOD        = (1U << 9),
    DIKU_EXTRA_ANTI_EVIL        = (1U << 10),
    DIKU_EXTRA_ANTI_NEUTRAL     = (1U << 11),
    DIKU_EXTRA_NOREMOVE         = (1U << 12),
    DIKU_EXTRA_INVENTORY        = (1U << 13),
    DIKU_EXTRA_NOPURGE          = (1U << 14),
    DIKU_EXTRA_ROT_DEATH        = (1U << 15),
    DIKU_EXTRA_VIS_DEATH        = (1U << 16),
    DIKU_EXTRA_NOSAC            = (1U << 17),
    DIKU_EXTRA_NONMETAL         = (1U << 18),
    DIKU_EXTRA_NOLOCATE         = (1U << 19),
    DIKU_EXTRA_MELT_DROP        = (1U << 20),
    DIKU_EXTRA_HAD_TIMER        = (1U << 21),
    DIKU_EXTRA_SELL_EXTRACT     = (1U << 22),
    DIKU_EXTRA_FLAMING          = (1U << 23),
    DIKU_EXTRA_CHAOS            = (1U << 24),
    DIKU_EXTRA_PROTOTYPE        = (1U << 25),
    DIKU_EXTRA_QUEST            = (1U << 26),
    DIKU_EXTRA_NO_DAMAGE        = (1U << 27),
    DIKU_EXTRA_NO_RESTRING      = (1U << 28)
};

/* ------------------------------------------------------------------ */
/* Native -> canonical conversion helpers                             */
/* ------------------------------------------------------------------ */
uint32_t diku_canonical_room_flags(diku_format_t fmt, uint32_t native);
uint32_t diku_canonical_act_flags(diku_format_t fmt, uint32_t native);
uint32_t diku_canonical_affect_flags(diku_format_t fmt, uint32_t native);
uint32_t diku_canonical_wear_flags(diku_format_t fmt, uint32_t native);
uint32_t diku_canonical_extra_flags(diku_format_t fmt, uint32_t native);

/* ------------------------------------------------------------------ */
/* Item type string -> canonical enum                                 */
/* ------------------------------------------------------------------ */
int diku_item_type_from_name(const char *name);

#ifdef __cplusplus
}
#endif

#ifdef DIKU_PARSER_IMPLEMENTATION

/* ------------------------------------------------------------------ */
/* Bitvector decoder implementation                                   */
/* ------------------------------------------------------------------ */
static inline uint32_t diku_decode_bitvector_ch(char c) {
    if (c >= 'A' && c <= 'Z') return (1U << (c - 'A'));
    if (c >= 'a' && c <= 'z') return (1U << (26 + (c - 'a')));
    return 0;
}

uint32_t diku_decode_bitvector(const char *s) {
    uint32_t flags = 0;
    if (!s) return 0;
    while (*s) {
        flags |= diku_decode_bitvector_ch(*s);
        s++;
    }
    return flags;
}

/* ------------------------------------------------------------------ */
/* Simple memmem replacement (avoids _GNU_SOURCE dependency)          */
/* ------------------------------------------------------------------ */
static const char *diku_memmem(const char *hay, size_t hlen, const char *needle, size_t nlen) {
    if (nlen == 0) return hay;
    if (hlen < nlen) return NULL;
    for (size_t i = 0; i <= hlen - nlen; i++) {
        if (memcmp(hay + i, needle, nlen) == 0) return hay + i;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Sniffer: inspect raw header/section signatures                     */
/* ------------------------------------------------------------------ */
static bool diku_is_word_boundary(char c) {
    return c == '\0' || isspace((unsigned char)c) || c == '~';
}

static const char *diku_find_header_end(const char *buf, size_t len) {
    /* Header ends at the first main entity section. Match whole tokens so
       that #RESETMSG does not false-positive as #RESETS. */
    static const char *markers[] = {
        "#MOBILES", "#MOBILE", "#OBJECTS", "#OBJECT", "#OBJ",
        "#ROOMS", "#ROOM", "#HELPS", "#RESETS", "#RESET",
        "#SHOPS", "#SHOP", "#SPECIALS", "#SPECIAL", "#OBJFUNS",
        "#OBJFUN", "#PROGS", "#REPAIRS"
    };
    const char *end = buf + len;
    const char *header_end = end;
    for (size_t i = 0; i < sizeof(markers) / sizeof(markers[0]); i++) {
        size_t mlen = strlen(markers[i]);
        const char *hit = buf;
        while ((hit = diku_memmem(hit, (size_t)(end - hit), markers[i], mlen)) != NULL) {
            if (diku_is_word_boundary(hit[mlen])) {
                if (hit < header_end) header_end = hit;
                break;
            }
            hit += 1;
        }
    }
    return header_end;
}

static diku_format_t diku_sniff_format_buf(const char *buf, size_t len) {
    if (!buf || len == 0) return DIKU_FMT_UNKNOWN;

    const char *header_end = diku_find_header_end(buf, len);
    size_t hlen = (size_t)(header_end - buf);

    /* SMAUG / custom keyword headers */
    if (diku_memmem(buf, hlen, "#AREADATA", 9) != NULL) return DIKU_FMT_SMAUG;
    if (diku_memmem(buf, hlen, "Name ", 5) != NULL &&
        diku_memmem(buf, hlen, "Builders ", 9) != NULL &&
        diku_memmem(buf, hlen, "VNUMs ", 6) != NULL)
        return DIKU_FMT_SMAUG;
    if (diku_memmem(buf, hlen, "Security ", 9) != NULL &&
        diku_memmem(buf, hlen, "#AREA", 5) != NULL)
        return DIKU_FMT_SMAUG;
    /* SMAUG derivatives use #RANGES / #RESETMSG / #FLAGS / #ECONOMY. */
    if (diku_memmem(buf, hlen, "#RANGES", 7) != NULL &&
        diku_memmem(buf, hlen, "#RESETMSG", 9) != NULL)
        return DIKU_FMT_SMAUG;
    if (diku_memmem(buf, hlen, "#FLAGS", 6) != NULL &&
        diku_memmem(buf, hlen, "#ECONOMY", 8) != NULL)
        return DIKU_FMT_SMAUG;
    /* Devil's Silence / custom SMAUG keyword block: Descr/Builders/Sec/Levels/Flags/End. */
    if ((diku_memmem(buf, hlen, "Descr", 5) != NULL ||
         diku_memmem(buf, hlen, "Description", 11) != NULL) &&
        diku_memmem(buf, hlen, "End", 3) != NULL &&
        (diku_memmem(buf, hlen, "Sec ", 4) != NULL ||
         diku_memmem(buf, hlen, "Security", 8) != NULL ||
         diku_memmem(buf, hlen, "Levels", 6) != NULL ||
         diku_memmem(buf, hlen, "Flags", 5) != NULL ||
         diku_memmem(buf, hlen, "Bldrs", 5) != NULL ||
         diku_memmem(buf, hlen, "Builders", 8) != NULL))
        return DIKU_FMT_SMAUG;

    /* ROM / Merc level range: #AREA ... {low high} ... within the header region.
       ROM uses two numbers (e.g. { 5 30}); Merc uses "{ All }" or similar. */
    const char *area = diku_memmem(buf, hlen, "#AREA", 5);
    if (area) {
        const char *p = area + 5;
        while (p < header_end) {
            while (p < header_end && *p != '{' && *p != '[') p++;
            if (p >= header_end) break;
            char opener = *p;
            char closer = (opener == '{') ? '}' : ']';
            p++;
            while (p < header_end && isspace((unsigned char)*p)) p++;
            const char *t1_start = p;
            while (p < header_end && !isspace((unsigned char)*p) && *p != closer) p++;
            const char *t1_end = p;
            while (p < header_end && isspace((unsigned char)*p)) p++;
            const char *t2_start = p;
            while (p < header_end && !isspace((unsigned char)*p) && *p != closer) p++;
            const char *t2_end = p;
            while (p < header_end && isspace((unsigned char)*p)) p++;
            if (p < header_end && *p == closer) {
                bool t1_num = true, t2_num = true;
                for (const char *q = t1_start; q < t1_end; q++)
                    if (!isdigit((unsigned char)*q)) { t1_num = false; break; }
                for (const char *q = t2_start; q < t2_end; q++)
                    if (!isdigit((unsigned char)*q)) { t2_num = false; break; }
                if (t1_num && t2_num) return DIKU_FMT_ROM;
                /* Non-numeric level range is a strong Merc indicator. */
                return DIKU_FMT_MERC;
            }
        }
    }

    /* Merc mobile signature: numeric act flags ending with an 'S' format marker
       and no ROM-style race/material lines. Look at the first mobile entry. */
    const char *mobiles = diku_memmem(buf, hlen, "#MOBILES", 8);
    if (mobiles) {
        const char *p = mobiles + 8;
        int mobiles_found = 0;
        while (p < header_end && mobiles_found < 1) {
            while (p < header_end && *p != '#') p++;
            if (p >= header_end) break;
            p++;
            /* Skip the vnum line. */
            while (p < header_end && *p != '\n' && *p != '\r') p++;
            while (p < header_end && (*p == '\n' || *p == '\r')) p++;
            /* Skip four tilde-terminated strings (name/short/long/desc). */
            int tildes = 0;
            while (p < header_end && tildes < 4) {
                if (*p == '~') tildes++;
                p++;
            }
            while (p < header_end && (*p == '\n' || *p == '\r' || isspace((unsigned char)*p))) p++;
            /* Now at the act_flags line. */
            const char *line_start = p;
            while (p < header_end && *p != '\n' && *p != '\r') p++;
            const char *line_end = p;
            /* Check if the line is numeric (allowing |) and ends with S. */
            bool looks_merc = true;
            bool has_s = false;
            for (const char *q = line_start; q < line_end; q++) {
                char c = *q;
                if (c == 'S' && (q + 1 == line_end || isspace((unsigned char)q[1]))) {
                    has_s = true;
                } else if (!isdigit((unsigned char)c) && !isspace((unsigned char)c) && c != '|' && c != '-') {
                    looks_merc = false;
                    break;
                }
            }
            if (looks_merc && has_s) return DIKU_FMT_MERC;
            mobiles_found++;
        }
    }

    /* CircleMUD package files are handled at the loader level; combined files
       are hard to distinguish from Merc without parsing, so leave to
       post-parse classification. */

    /* Merc / DikuMUD: plural section tags and numeric flags. Default to Merc
       if we see mobile/object sections, Diku otherwise. */
    if (diku_memmem(buf, hlen, "#MOBILES", 8) != NULL ||
        diku_memmem(buf, hlen, "#OBJECTS", 8) != NULL ||
        diku_memmem(buf, hlen, "#ROOMS", 6) != NULL) {
        return DIKU_FMT_MERC;
    }

    return DIKU_FMT_DIKU;
}

diku_format_t diku_sniff_format(const char *buf, size_t len) {
    return diku_sniff_format_buf(buf, len);
}

/* ------------------------------------------------------------------ */
/* Post-parse classifier                                              */
/* ------------------------------------------------------------------ */
int diku_format_priority(diku_format_t fmt) {
    switch (fmt) {
        case DIKU_FMT_SMAUG:  return 60;
        case DIKU_FMT_ROM:    return 50;
        case DIKU_FMT_CIRCLE: return 40;
        case DIKU_FMT_MERC:   return 30;
        case DIKU_FMT_DIKU:   return 20;
        case DIKU_FMT_CUSTOM: return 10;
        default:              return 0;
    }
}

diku_format_t diku_classify_area(const area_t *area) {
    if (!area) return DIKU_FMT_UNKNOWN;

    /* SMAUG: keyword header fields or security/version. */
    if (area->security > 0 || area->version > 0)
        return DIKU_FMT_SMAUG;

    /* ROM: material fields or explicit item levels are strong indicators. */
    bool has_material = false;
    for (int i = 0; i < area->mobile_count && !has_material; i++)
        if (area->mobiles[i].material.len > 0) has_material = true;
    for (int i = 0; i < area->item_count && !has_material; i++)
        if (area->items[i].material.len > 0)
            has_material = true;
    if (has_material)
        return DIKU_FMT_ROM;

    /* Merc: has both rooms and mobiles. */
    if (area->room_count > 0 && area->mobile_count > 0)
        return DIKU_FMT_MERC;

    /* DikuMUD: minimal numeric files. */
    if (area->room_count > 0 || area->mobile_count > 0 || area->item_count > 0)
        return DIKU_FMT_DIKU;

    return DIKU_FMT_UNKNOWN;
}

/* ------------------------------------------------------------------ */
/* Parse a numeric token that may contain '|' joined flags            */
/* ------------------------------------------------------------------ */
static uint32_t diku_parse_numeric_flags(const char *s) {
    uint32_t flags = 0;
    if (!s || !*s) return 0;
    const char *p = s;
    while (*p) {
        while (*p && !isdigit((unsigned char)*p) && *p != '-') p++;
        if (!*p) break;
        char *end;
        long n = strtol(p, &end, 10);
        if (end == p) break;
        flags |= (uint32_t)n;
        p = end;
    }
    return flags;
}

/* ------------------------------------------------------------------ */
/* Native -> canonical flag conversion tables                         */
/* ------------------------------------------------------------------ */

/* Room flags: most forks map the same semantic bits, but SMAUG extends them
   and Circle/Merc use some divergent high bits. Provide explicit tables. */
static uint32_t diku_convert_rom_room_flags(uint32_t native) {
    /* ROM alphabetic bits map 1:1 to canonical low bits for the common set. */
    uint32_t out = 0;
    if (native & (1U << 0))  out |= DIKU_ROOM_DARK;
    if (native & (1U << 1))  out |= DIKU_ROOM_NO_MOB;
    if (native & (1U << 2))  out |= DIKU_ROOM_INDOORS;
    if (native & (1U << 3))  out |= DIKU_ROOM_PEACEFUL;
    if (native & (1U << 4))  out |= DIKU_ROOM_STEAL;
    if (native & (1U << 5))  out |= DIKU_ROOM_NO_MAGIC;
    if (native & (1U << 6))  out |= DIKU_ROOM_TUNNEL;
    if (native & (1U << 7))  out |= DIKU_ROOM_PRIVATE;
    if (native & (1U << 8))  out |= DIKU_ROOM_GODS_ONLY;
    if (native & (1U << 9))  out |= DIKU_ROOM_NO_RECALL;
    if (native & (1U << 10)) out |= DIKU_ROOM_IMP_ONLY;
    if (native & (1U << 11)) out |= DIKU_ROOM_GOD_ONLY;
    if (native & (1U << 12)) out |= DIKU_ROOM_HEROES_ONLY;
    if (native & (1U << 13)) out |= DIKU_ROOM_NEWBIES_ONLY;
    if (native & (1U << 14)) out |= DIKU_ROOM_LAW;
    if (native & (1U << 15)) out |= DIKU_ROOM_NOWHERE;
    /* high bits for ROM 2.4+ / SMAUG extensions */
    if (native & (1U << 20)) out |= DIKU_ROOM_SILENCE;
    if (native & (1U << 21)) out |= DIKU_ROOM_LOGSPEECH;
    if (native & (1U << 22)) out |= DIKU_ROOM_NOCAMP;
    if (native & (1U << 23)) out |= DIKU_ROOM_NOSUMMON;
    if (native & (1U << 24)) out |= DIKU_ROOM_NOASTRAL;
    return out;
}

static uint32_t diku_convert_merc_room_flags(uint32_t native) {
    /* Merc numeric room flags mostly match canonical positions. */
    uint32_t out = 0;
    if (native & (1U << 0))  out |= DIKU_ROOM_DARK;
    if (native & (1U << 1))  out |= DIKU_ROOM_NO_MOB;
    if (native & (1U << 2))  out |= DIKU_ROOM_INDOORS;
    if (native & (1U << 3))  out |= DIKU_ROOM_PEACEFUL;
    if (native & (1U << 4))  out |= DIKU_ROOM_STEAL;
    if (native & (1U << 5))  out |= DIKU_ROOM_NO_MAGIC;
    if (native & (1U << 6))  out |= DIKU_ROOM_TUNNEL;
    if (native & (1U << 7))  out |= DIKU_ROOM_PRIVATE;
    if (native & (1U << 8))  out |= DIKU_ROOM_GODS_ONLY;
    if (native & (1U << 9))  out |= DIKU_ROOM_NO_RECALL;
    return out;
}

uint32_t diku_canonical_room_flags(diku_format_t fmt, uint32_t native) {
    switch (fmt) {
        case DIKU_FMT_ROM:
        case DIKU_FMT_SMAUG:
            return diku_convert_rom_room_flags(native);
        case DIKU_FMT_MERC:
        case DIKU_FMT_DIKU:
        case DIKU_FMT_CIRCLE:
            return diku_convert_merc_room_flags(native);
        default:
            return native;
    }
}

/* Act flags: this is where Merc and ROM diverge significantly. */
static uint32_t diku_convert_merc_act_flags(uint32_t native) {
    uint32_t out = 0;
    if (native & 0x00001) out |= DIKU_ACT_IS_NPC;
    if (native & 0x00002) out |= DIKU_ACT_SENTINEL;
    if (native & 0x00004) out |= DIKU_ACT_SCAVENGER;
    if (native & 0x00010) out |= DIKU_ACT_AGGRESSIVE;
    if (native & 0x00020) out |= DIKU_ACT_STAY_AREA;
    if (native & 0x00040) out |= DIKU_ACT_WIMPY;
    if (native & 0x00080) out |= DIKU_ACT_PET;
    if (native & 0x00100) out |= DIKU_ACT_TRAIN;
    if (native & 0x00200) out |= DIKU_ACT_PRACTICE;
    if (native & 0x00400) out |= DIKU_ACT_UNDEAD;
    if (native & 0x00800) out |= DIKU_ACT_CLERIC;
    if (native & 0x01000) out |= DIKU_ACT_MAGE;
    if (native & 0x02000) out |= DIKU_ACT_THIEF;
    if (native & 0x04000) out |= DIKU_ACT_WARRIOR;
    if (native & 0x08000) out |= DIKU_ACT_NOALIGN;
    if (native & 0x10000) out |= DIKU_ACT_NOPURGE;
    if (native & 0x20000) out |= DIKU_ACT_OUTDOORS;
    if (native & 0x40000) out |= DIKU_ACT_INDOORS;
    if (native & 0x80000) out |= DIKU_ACT_IS_HEALER;
    return out;
}

static uint32_t diku_convert_rom_act_flags(uint32_t native) {
    uint32_t out = 0;
    if (native & (1U << 0))  out |= DIKU_ACT_IS_NPC;
    if (native & (1U << 1))  out |= DIKU_ACT_SENTINEL;
    if (native & (1U << 2))  out |= DIKU_ACT_SCAVENGER;
    if (native & (1U << 4))  out |= DIKU_ACT_AGGRESSIVE;
    if (native & (1U << 5))  out |= DIKU_ACT_STAY_AREA;
    if (native & (1U << 6))  out |= DIKU_ACT_WIMPY;
    if (native & (1U << 7))  out |= DIKU_ACT_PET;
    if (native & (1U << 8))  out |= DIKU_ACT_TRAIN;
    if (native & (1U << 9))  out |= DIKU_ACT_PRACTICE;
    if (native & (1U << 10)) out |= DIKU_ACT_UNDEAD;
    if (native & (1U << 11)) out |= DIKU_ACT_CLERIC;
    if (native & (1U << 12)) out |= DIKU_ACT_MAGE;
    if (native & (1U << 13)) out |= DIKU_ACT_THIEF;
    if (native & (1U << 14)) out |= DIKU_ACT_WARRIOR;
    if (native & (1U << 15)) out |= DIKU_ACT_NOALIGN;
    if (native & (1U << 16)) out |= DIKU_ACT_NOPURGE;
    if (native & (1U << 17)) out |= DIKU_ACT_OUTDOORS;
    if (native & (1U << 18)) out |= DIKU_ACT_INDOORS;
    if (native & (1U << 19)) out |= DIKU_ACT_IS_HEALER;
    if (native & (1U << 20)) out |= DIKU_ACT_GAIN;
    if (native & (1U << 21)) out |= DIKU_ACT_UPDATE_ALWAYS;
    if (native & (1U << 22)) out |= DIKU_ACT_IS_CHANGER;
    if (native & (1U << 23)) out |= DIKU_ACT_AGGRESSIVE_EVIL;
    if (native & (1U << 24)) out |= DIKU_ACT_AGGRESSIVE_GOOD;
    if (native & (1U << 25)) out |= DIKU_ACT_AGGRESSIVE_NEUTRAL;
    if (native & (1U << 26)) out |= DIKU_ACT_WERE;
    if (native & (1U << 27)) out |= DIKU_ACT_MOUNT;
    return out;
}

uint32_t diku_canonical_act_flags(diku_format_t fmt, uint32_t native) {
    switch (fmt) {
        case DIKU_FMT_ROM:
        case DIKU_FMT_SMAUG:
            return diku_convert_rom_act_flags(native);
        case DIKU_FMT_MERC:
        case DIKU_FMT_DIKU:
        case DIKU_FMT_CIRCLE:
            return diku_convert_merc_act_flags(native);
        default:
            return native;
    }
}

/* Affect flags: positions differ between Merc and ROM. */
static uint32_t diku_convert_merc_affect_flags(uint32_t native) {
    uint32_t out = 0;
    if (native & (1U << 0))  out |= DIKU_AFF_BLIND;
    if (native & (1U << 1))  out |= DIKU_AFF_INVISIBLE;
    if (native & (1U << 2))  out |= DIKU_AFF_DETECT_EVIL;
    if (native & (1U << 3))  out |= DIKU_AFF_DETECT_INVIS;
    if (native & (1U << 4))  out |= DIKU_AFF_DETECT_MAGIC;
    if (native & (1U << 5))  out |= DIKU_AFF_DETECT_HIDDEN;
    if (native & (1U << 6))  out |= DIKU_AFF_HOLD;
    if (native & (1U << 7))  out |= DIKU_AFF_SANCTUARY;
    if (native & (1U << 8))  out |= DIKU_AFF_FAERIE_FIRE;
    if (native & (1U << 9))  out |= DIKU_AFF_INFRARED;
    if (native & (1U << 10)) out |= DIKU_AFF_CURSE;
    if (native & (1U << 11)) out |= DIKU_AFF_NO_FLAG;
    if (native & (1U << 12)) out |= DIKU_AFF_POISON;
    if (native & (1U << 13)) out |= DIKU_AFF_PROTECT_EVIL;
    if (native & (1U << 14)) out |= DIKU_AFF_PROTECT_GOOD;
    if (native & (1U << 15)) out |= DIKU_AFF_SNEAK;
    if (native & (1U << 16)) out |= DIKU_AFF_HIDE;
    if (native & (1U << 17)) out |= DIKU_AFF_SLEEP;
    if (native & (1U << 18)) out |= DIKU_AFF_CHARM;
    if (native & (1U << 19)) out |= DIKU_AFF_FLYING;
    if (native & (1U << 20)) out |= DIKU_AFF_PASS_DOOR;
    return out;
}

static uint32_t diku_convert_rom_affect_flags(uint32_t native) {
    uint32_t out = 0;
    if (native & (1U << 0))  out |= DIKU_AFF_BLIND;
    if (native & (1U << 1))  out |= DIKU_AFF_INVISIBLE;
    if (native & (1U << 2))  out |= DIKU_AFF_DETECT_EVIL;
    if (native & (1U << 3))  out |= DIKU_AFF_DETECT_INVIS;
    if (native & (1U << 4))  out |= DIKU_AFF_DETECT_MAGIC;
    if (native & (1U << 5))  out |= DIKU_AFF_DETECT_HIDDEN;
    if (native & (1U << 6))  out |= DIKU_AFF_HOLD;
    if (native & (1U << 7))  out |= DIKU_AFF_SANCTUARY;
    if (native & (1U << 8))  out |= DIKU_AFF_FAERIE_FIRE;
    if (native & (1U << 9))  out |= DIKU_AFF_INFRARED;
    if (native & (1U << 10)) out |= DIKU_AFF_CURSE;
    if (native & (1U << 11)) out |= DIKU_AFF_NO_FLAG;
    if (native & (1U << 12)) out |= DIKU_AFF_POISON;
    if (native & (1U << 13)) out |= DIKU_AFF_PROTECT_EVIL;
    if (native & (1U << 14)) out |= DIKU_AFF_PROTECT_GOOD;
    if (native & (1U << 15)) out |= DIKU_AFF_SNEAK;
    if (native & (1U << 16)) out |= DIKU_AFF_HIDE;
    if (native & (1U << 17)) out |= DIKU_AFF_SLEEP;
    if (native & (1U << 18)) out |= DIKU_AFF_CHARM;
    if (native & (1U << 19)) out |= DIKU_AFF_FLYING;
    if (native & (1U << 20)) out |= DIKU_AFF_PASS_DOOR;
    if (native & (1U << 21)) out |= DIKU_AFF_HASTE;
    if (native & (1U << 22)) out |= DIKU_AFF_CALM;
    if (native & (1U << 23)) out |= DIKU_AFF_PLAGUE;
    if (native & (1U << 24)) out |= DIKU_AFF_WEAKEN;
    if (native & (1U << 25)) out |= DIKU_AFF_DARK_VISION;
    if (native & (1U << 26)) out |= DIKU_AFF_BERSERK;
    if (native & (1U << 27)) out |= DIKU_AFF_SWIM;
    if (native & (1U << 28)) out |= DIKU_AFF_REGENERATION;
    if (native & (1U << 29)) out |= DIKU_AFF_SLOW;
    return out;
}

uint32_t diku_canonical_affect_flags(diku_format_t fmt, uint32_t native) {
    switch (fmt) {
        case DIKU_FMT_ROM:
        case DIKU_FMT_SMAUG:
            return diku_convert_rom_affect_flags(native);
        case DIKU_FMT_MERC:
        case DIKU_FMT_DIKU:
        case DIKU_FMT_CIRCLE:
            return diku_convert_merc_affect_flags(native);
        default:
            return native;
    }
}

/* Wear flags: alphabetic and numeric positions are aligned. */
uint32_t diku_canonical_wear_flags(diku_format_t fmt, uint32_t native) {
    (void)fmt;
    uint32_t out = 0;
    if (native & (1U << 0))  out |= DIKU_WEAR_TAKE;
    if (native & (1U << 1))  out |= DIKU_WEAR_FINGER;
    if (native & (1U << 2))  out |= DIKU_WEAR_NECK;
    if (native & (1U << 3))  out |= DIKU_WEAR_BODY;
    if (native & (1U << 4))  out |= DIKU_WEAR_HEAD;
    if (native & (1U << 5))  out |= DIKU_WEAR_LEGS;
    if (native & (1U << 6))  out |= DIKU_WEAR_FEET;
    if (native & (1U << 7))  out |= DIKU_WEAR_HANDS;
    if (native & (1U << 8))  out |= DIKU_WEAR_ARMS;
    if (native & (1U << 9))  out |= DIKU_WEAR_SHIELD;
    if (native & (1U << 10)) out |= DIKU_WEAR_ABOUT;
    if (native & (1U << 11)) out |= DIKU_WEAR_WAIST;
    if (native & (1U << 12)) out |= DIKU_WEAR_WRIST;
    if (native & (1U << 13)) out |= DIKU_WEAR_WIELD;
    if (native & (1U << 14)) out |= DIKU_WEAR_HOLD;
    if (native & (1U << 15)) out |= DIKU_WEAR_TWO_HANDS;
    if (native & (1U << 16)) out |= DIKU_WEAR_EARS;
    if (native & (1U << 17)) out |= DIKU_WEAR_EYES;
    if (native & (1U << 18)) out |= DIKU_WEAR_LIGHT;
    if (native & (1U << 19)) out |= DIKU_WEAR_BACK;
    if (native & (1U << 20)) out |= DIKU_WEAR_FACE;
    if (native & (1U << 21)) out |= DIKU_WEAR_ANKLE;
    return out;
}

/* Extra flags: alphabetic and numeric positions are aligned. */
uint32_t diku_canonical_extra_flags(diku_format_t fmt, uint32_t native) {
    (void)fmt;
    uint32_t out = 0;
    if (native & (1U << 0))  out |= DIKU_EXTRA_GLOW;
    if (native & (1U << 1))  out |= DIKU_EXTRA_HUM;
    if (native & (1U << 2))  out |= DIKU_EXTRA_DARK;
    if (native & (1U << 3))  out |= DIKU_EXTRA_LOCK;
    if (native & (1U << 4))  out |= DIKU_EXTRA_EVIL;
    if (native & (1U << 5))  out |= DIKU_EXTRA_INVIS;
    if (native & (1U << 6))  out |= DIKU_EXTRA_MAGIC;
    if (native & (1U << 7))  out |= DIKU_EXTRA_NODROP;
    if (native & (1U << 8))  out |= DIKU_EXTRA_BLESS;
    if (native & (1U << 9))  out |= DIKU_EXTRA_ANTI_GOOD;
    if (native & (1U << 10)) out |= DIKU_EXTRA_ANTI_EVIL;
    if (native & (1U << 11)) out |= DIKU_EXTRA_ANTI_NEUTRAL;
    if (native & (1U << 12)) out |= DIKU_EXTRA_NOREMOVE;
    if (native & (1U << 13)) out |= DIKU_EXTRA_INVENTORY;
    if (native & (1U << 14)) out |= DIKU_EXTRA_NOPURGE;
    if (native & (1U << 15)) out |= DIKU_EXTRA_ROT_DEATH;
    if (native & (1U << 16)) out |= DIKU_EXTRA_VIS_DEATH;
    if (native & (1U << 17)) out |= DIKU_EXTRA_NOSAC;
    if (native & (1U << 18)) out |= DIKU_EXTRA_NONMETAL;
    if (native & (1U << 19)) out |= DIKU_EXTRA_NOLOCATE;
    if (native & (1U << 20)) out |= DIKU_EXTRA_MELT_DROP;
    if (native & (1U << 21)) out |= DIKU_EXTRA_HAD_TIMER;
    if (native & (1U << 22)) out |= DIKU_EXTRA_SELL_EXTRACT;
    if (native & (1U << 23)) out |= DIKU_EXTRA_FLAMING;
    if (native & (1U << 24)) out |= DIKU_EXTRA_CHAOS;
    if (native & (1U << 25)) out |= DIKU_EXTRA_PROTOTYPE;
    if (native & (1U << 26)) out |= DIKU_EXTRA_QUEST;
    if (native & (1U << 27)) out |= DIKU_EXTRA_NO_DAMAGE;
    if (native & (1U << 28)) out |= DIKU_EXTRA_NO_RESTRING;
    return out;
}

/* ------------------------------------------------------------------ */
/* Item type name -> canonical enum                                   */
/* ------------------------------------------------------------------ */
int diku_item_type_from_name(const char *name) {
    if (!name || !*name) return 0;
    static const struct { const char *name; int type; } table[] = {
        {"light",      DIKU_ITEM_LIGHT},
        {"scroll",     DIKU_ITEM_SCROLL},
        {"wand",       DIKU_ITEM_WAND},
        {"staff",      DIKU_ITEM_STAFF},
        {"weapon",     DIKU_ITEM_WEAPON},
        {"treasure",   DIKU_ITEM_TREASURE},
        {"armor",      DIKU_ITEM_ARMOR},
        {"potion",     DIKU_ITEM_POTION},
        {"furniture",  DIKU_ITEM_FURNITURE},
        {"trash",      DIKU_ITEM_TRASH},
        {"container",  DIKU_ITEM_CONTAINER},
        {"drink",      DIKU_ITEM_DRINK_CON},
        {"drinkcon",   DIKU_ITEM_DRINK_CON},
        {"drink_con",  DIKU_ITEM_DRINK_CON},
        {"key",        DIKU_ITEM_KEY},
        {"food",       DIKU_ITEM_FOOD},
        {"money",      DIKU_ITEM_MONEY},
        {"boat",       DIKU_ITEM_BOAT},
        {"corpse_npc", DIKU_ITEM_CORPSE_NPC},
        {"corpse_pc",  DIKU_ITEM_CORPSE_PC},
        {"fountain",   DIKU_ITEM_FOUNTAIN},
        {"pill",       DIKU_ITEM_PILL},
        {NULL, 0}
    };
    for (int i = 0; table[i].name; i++) {
        if (strcasecmp(name, table[i].name) == 0) return table[i].type;
    }
    /* Fallback: if it looks like a number, return it directly. */
    char *end;
    long n = strtol(name, &end, 10);
    if (end != name && *end == '\0') return (int)n;
    return 0;
}

#endif /* DIKU_PARSER_IMPLEMENTATION */

#endif /* DIKU_SCHEMA_H */
