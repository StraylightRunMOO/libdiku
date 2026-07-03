# DikuMUD Universal Schema

This document describes the canonical flag and value schema used by `include/diku/schema.h`. The schema provides fork-independent constants and conversion helpers so that code consuming parsed area files can interpret flags consistently whether the source file came from DikuMUD, Merc, ROM, SMAUG, or a custom derivative.

## Fork Detection

Every loaded `area_t` stores its detected fork in `area->format` (`diku_format_t`). Detection is hybrid:

1. **Pre-parse sniffer** (`diku_sniff_format`) inspects raw header and section signatures.
2. **Post-parse classifier** (`diku_classify_area`) refines the guess using populated data.

| Fork | Key Signals |
|------|-------------|
| `DIKU_FMT_DIKU` | Minimal `#AREA` header, no level range, numeric flags. |
| `DIKU_FMT_MERC` | `{ All }` level range, numeric `S`-format mobiles, pipe-joined flags (`2|512`). |
| `DIKU_FMT_ROM` | `{low high}` numeric level range, race/material lines, alphabetic bitvectors. |
| `DIKU_FMT_SMAUG` | `#AREADATA` or keyword headers (`Name`, `Builders`, `VNUMs`, `Security`), singular section tags. |
| `DIKU_FMT_CIRCLE` | Multi-file `.wld`/`.mob`/`.obj`/`.zon` packages. |
| `DIKU_FMT_CUSTOM` | Explicitly marked custom/hybrid formats. |

## Canonical Flag Conversions

Use these helpers to convert native fork-specific flag values to canonical bits:

```c
uint32_t diku_canonical_room_flags(diku_format_t fmt, uint32_t native);
uint32_t diku_canonical_act_flags(diku_format_t fmt, uint32_t native);
uint32_t diku_canonical_affect_flags(diku_format_t fmt, uint32_t native);
uint32_t diku_canonical_wear_flags(diku_format_t fmt, uint32_t native);
uint32_t diku_canonical_extra_flags(diku_format_t fmt, uint32_t native);
```

Example:

```c
uint32_t canon = diku_canonical_act_flags(area->format, mob->act_flags);
if (canon & DIKU_ACT_AGGRESSIVE) { /* mobile is aggressive */ }
```

## Canonical Room Flags

| Canonical Constant | ROM Letter | Merc/Circle Numeric | Description |
|--------------------|------------|---------------------|-------------|
| `DIKU_ROOM_DARK` | `A` | `1 << 0` | Requires a light source. |
| `DIKU_ROOM_NO_MOB` | `B` | `1 << 1` | Mobs cannot enter. |
| `DIKU_ROOM_INDOORS` | `C` | `1 << 2` | Inside; blocks weather. |
| `DIKU_ROOM_PEACEFUL` | `D` | `1 << 3` | Combat disabled. |
| `DIKU_ROOM_STEAL` | `E` | `1 << 4` | Stealing allowed/handled. |
| `DIKU_ROOM_NO_MAGIC` | `F` | `1 << 5` | Magic cannot be cast. |
| `DIKU_ROOM_TUNNEL` | `G` | `1 << 6` | Limited player capacity. |
| `DIKU_ROOM_PRIVATE` | `H` | `1 << 7` | Maximum of 2 characters. |
| `DIKU_ROOM_GODS_ONLY` | `I` | `1 << 8` | Immortal-only. |
| `DIKU_ROOM_NO_RECALL` | `J` | | Recall disabled. |
| `DIKU_ROOM_IMP_ONLY` | `K` | | Implementor-only. |
| `DIKU_ROOM_GOD_ONLY` | `L` | | God-only. |
| `DIKU_ROOM_HEROES_ONLY` | `M` | | Hero-level only. |
| `DIKU_ROOM_NEWBIES_ONLY` | `N` | | Newbie-only. |
| `DIKU_ROOM_LAW` | `O` | | Lawful room. |
| `DIKU_ROOM_NOWHERE` | `P` | | Virtual/nowhere room. |
| `DIKU_ROOM_SILENCE` | `S` | | Speech blocked. |
| `DIKU_ROOM_LOGSPEECH` | `T` | | Logs speech. |
| `DIKU_ROOM_NOCAMP` | `U` | | Camping disabled. |
| `DIKU_ROOM_NOSUMMON` | `V` | | Summon disabled. |
| `DIKU_ROOM_NOASTRAL` | `W` | | Astral travel disabled. |

## Canonical Mobile Act Flags

| Canonical Constant | ROM Letter | Merc Numeric | Description |
|--------------------|------------|--------------|-------------|
| `DIKU_ACT_IS_NPC` | `A` | `1` | Mobile (always set in area files). |
| `DIKU_ACT_SENTINEL` | `B` | `2` | Does not wander. |
| `DIKU_ACT_SCAVENGER` | `C` | `4` | Picks up items. |
| `DIKU_ACT_AGGRESSIVE` | `E` | `16` | Attacks players on sight. |
| `DIKU_ACT_STAY_AREA` | `F` | `32` | Does not leave home zone. |
| `DIKU_ACT_WIMPY` | `G` | `64` | Flees when hurt. |
| `DIKU_ACT_PET` | `H` | `128` | Can be bought as pet. |
| `DIKU_ACT_TRAIN` | `I` | `256` | Trains attributes. |
| `DIKU_ACT_PRACTICE` | `J` | `512` | Practices skills. |
| `DIKU_ACT_UNDEAD` | | `1024` | Undead creature. |
| `DIKU_ACT_CLERIC` | | `2048` | Cleric behavior. |
| `DIKU_ACT_MAGE` | | `4096` | Mage behavior. |
| `DIKU_ACT_THIEF` | | `8192` | Thief behavior. |
| `DIKU_ACT_WARRIOR` | `Z` | `16384` | Warrior behavior. |
| `DIKU_ACT_NOALIGN` | | `32768` | Alignment immune. |
| `DIKU_ACT_NOPURGE` | | `65536` | Not purged. |
| `DIKU_ACT_OUTDOORS` | | `131072` | Stays outdoors. |
| `DIKU_ACT_INDOORS` | | `262144` | Stays indoors. |
| `DIKU_ACT_IS_HEALER` | | `524288` | Healer services. |
| `DIKU_ACT_GAIN` | | | Skill gain trainer. |
| `DIKU_ACT_UPDATE_ALWAYS` | | | Always updated. |
| `DIKU_ACT_IS_CHANGER` | | | Money changer. |
| `DIKU_ACT_AGGRESSIVE_EVIL` | `U` | | Attacks evil alignment. |
| `DIKU_ACT_AGGRESSIVE_GOOD` | `V` | | Attacks good alignment. |
| `DIKU_ACT_AGGRESSIVE_NEUTRAL` | `W` | | Attacks neutral alignment. |
| `DIKU_ACT_WERE` | | | Lycanthrope. |
| `DIKU_ACT_MOUNT` | | | Mountable. |

## Canonical Wear Flags

Wear flag positions are aligned across forks (ROM letters and Merc/Circle numbers map to the same bits).

| Canonical Constant | ROM Letter | Merc/Circle Numeric |
|--------------------|------------|---------------------|
| `DIKU_WEAR_TAKE` | `A` | `1` |
| `DIKU_WEAR_FINGER` | `B` | `2` |
| `DIKU_WEAR_NECK` | `C` | `4` |
| `DIKU_WEAR_BODY` | `D` | `8` |
| `DIKU_WEAR_HEAD` | `E` | `16` |
| `DIKU_WEAR_LEGS` | `F` | `32` |
| `DIKU_WEAR_FEET` | `G` | `64` |
| `DIKU_WEAR_HANDS` | `H` | `128` |
| `DIKU_WEAR_ARMS` | `I` | `256` |
| `DIKU_WEAR_SHIELD` | `J` | `512` |
| `DIKU_WEAR_ABOUT` | `K` | `1024` |
| `DIKU_WEAR_WAIST` | `L` | `2048` |
| `DIKU_WEAR_WRIST` | `M` | `4096` |
| `DIKU_WEAR_WIELD` | `N` | `8192` |
| `DIKU_WEAR_HOLD` | `O` | `16384` |
| `DIKU_WEAR_TWO_HANDS` | `P` | `32768` |
| `DIKU_WEAR_EARS` | `Q` | `65536` |
| `DIKU_WEAR_EYES` | `R` | `131072` |
| `DIKU_WEAR_LIGHT` | `S` | `262144` |
| `DIKU_WEAR_BACK` | `T` | `524288` |
| `DIKU_WEAR_FACE` | `U` | `1048576` |
| `DIKU_WEAR_ANKLE` | `V` | `2097152` |

## Canonical Extra Flags

| Canonical Constant | ROM Letter | Merc Numeric | Description |
|--------------------|------------|--------------|-------------|
| `DIKU_EXTRA_GLOW` | `A` | `1` | Glows. |
| `DIKU_EXTRA_HUM` | `B` | `2` | Hums. |
| `DIKU_EXTRA_DARK` | `C` | `4` | Dark. |
| `DIKU_EXTRA_LOCK` | `D` | `8` | Locked. |
| `DIKU_EXTRA_EVIL` | `E` | `16` | Evil aura. |
| `DIKU_EXTRA_INVIS` | `F` | `32` | Invisible. |
| `DIKU_EXTRA_MAGIC` | `G` | `64` | Magic. |
| `DIKU_EXTRA_NODROP` | `H` | `128` | Cannot drop. |
| `DIKU_EXTRA_BLESS` | `I` | `256` | Blessed. |
| `DIKU_EXTRA_ANTI_GOOD` | `J` | `512` | Anti-good. |
| `DIKU_EXTRA_ANTI_EVIL` | `K` | `1024` | Anti-evil. |
| `DIKU_EXTRA_ANTI_NEUTRAL` | `L` | `2048` | Anti-neutral. |
| `DIKU_EXTRA_NOREMOVE` | `M` | `4096` | Cannot remove. |
| `DIKU_EXTRA_INVENTORY` | `N` | `8192` | Inventory-only. |
| `DIKU_EXTRA_NOPURGE` | `O` | | Not purged. |
| `DIKU_EXTRA_ROT_DEATH` | `P` | | Rots on death. |
| `DIKU_EXTRA_VIS_DEATH` | `Q` | | Visible after death. |
| `DIKU_EXTRA_NOSAC` | `R` | | Cannot sacrifice. |
| `DIKU_EXTRA_NONMETAL` | `S` | | Non-metal. |
| `DIKU_EXTRA_NOLOCATE` | `T` | | Cannot locate. |
| `DIKU_EXTRA_MELT_DROP` | `U` | | Melts when dropped. |
| `DIKU_EXTRA_HAD_TIMER` | `V` | | Had rent timer. |
| `DIKU_EXTRA_SELL_EXTRACT` | `W` | | Extracted on sell. |
| `DIKU_EXTRA_FLAMING` | `X` | | Flaming. |
| `DIKU_EXTRA_CHAOS` | `Y` | | Chaos. |
| `DIKU_EXTRA_PROTOTYPE` | `Z` | | Prototype. |
| `DIKU_EXTRA_QUEST` | `aa` | | Quest item. |
| `DIKU_EXTRA_NO_DAMAGE` | `bb` | | Indestructible. |
| `DIKU_EXTRA_NO_RESTRING` | `cc` | | No restringing. |

## Canonical Affect Flags

| Canonical Constant | ROM Letter | Merc Numeric | Description |
|--------------------|------------|--------------|-------------|
| `DIKU_AFF_BLIND` | `A` | `1` | Blind. |
| `DIKU_AFF_INVISIBLE` | `B` | `2` | Invisible. |
| `DIKU_AFF_DETECT_EVIL` | `C` | `4` | Detect evil. |
| `DIKU_AFF_DETECT_INVIS` | `D` | `8` | Detect invisible. |
| `DIKU_AFF_DETECT_MAGIC` | `E` | `16` | Detect magic. |
| `DIKU_AFF_DETECT_HIDDEN` | `F` | `32` | Detect hidden. |
| `DIKU_AFF_HOLD` | `G` | `64` | Held. |
| `DIKU_AFF_SANCTUARY` | `H` | `128` | Sanctuary. |
| `DIKU_AFF_FAERIE_FIRE` | `I` | `256` | Faerie fire. |
| `DIKU_AFF_INFRARED` | `J` | `512` | Infravision. |
| `DIKU_AFF_CURSE` | `K` | `1024` | Cursed. |
| `DIKU_AFF_NO_FLAG` | `L` | `2048` | Reserved/no flag. |
| `DIKU_AFF_POISON` | `M` | `4096` | Poisoned. |
| `DIKU_AFF_PROTECT_EVIL` | `N` | `8192` | Protection from evil. |
| `DIKU_AFF_PROTECT_GOOD` | `O` | `16384` | Protection from good. |
| `DIKU_AFF_SNEAK` | `P` | `32768` | Sneaking. |
| `DIKU_AFF_HIDE` | `Q` | `65536` | Hidden. |
| `DIKU_AFF_SLEEP` | `R` | `131072` | Sleeping. |
| `DIKU_AFF_CHARM` | `S` | `262144` | Charmed. |
| `DIKU_AFF_FLYING` | `T` | `524288` | Flying. |
| `DIKU_AFF_PASS_DOOR` | `U` | `1048576` | Pass door. |
| `DIKU_AFF_HASTE` | `V` | | Hasted. |
| `DIKU_AFF_CALM` | `W` | | Calmed. |
| `DIKU_AFF_PLAGUE` | `X` | | Plagued. |
| `DIKU_AFF_WEAKEN` | `Y` | | Weakened. |
| `DIKU_AFF_DARK_VISION` | `Z` | | Dark vision. |
| `DIKU_AFF_BERSERK` | `aa` | | Berserk. |
| `DIKU_AFF_SWIM` | `bb` | | Water breathing/swim. |
| `DIKU_AFF_REGENERATION` | `cc` | | Regeneration. |
| `DIKU_AFF_SLOW` | `dd` | | Slowed. |

## Canonical Item Types

| Constant | Value | Description |
|----------|-------|-------------|
| `DIKU_ITEM_LIGHT` | 1 | Light source. |
| `DIKU_ITEM_SCROLL` | 2 | Scroll. |
| `DIKU_ITEM_WAND` | 3 | Wand. |
| `DIKU_ITEM_STAFF` | 4 | Staff. |
| `DIKU_ITEM_WEAPON` | 5 | Weapon. |
| `DIKU_ITEM_TREASURE` | 8 | Treasure. |
| `DIKU_ITEM_ARMOR` | 9 | Armor. |
| `DIKU_ITEM_POTION` | 10 | Potion. |
| `DIKU_ITEM_FURNITURE` | 12 | Furniture. |
| `DIKU_ITEM_TRASH` | 13 | Trash. |
| `DIKU_ITEM_CONTAINER` | 15 | Container. |
| `DIKU_ITEM_DRINK_CON` | 17 | Drink container. |
| `DIKU_ITEM_KEY` | 18 | Key. |
| `DIKU_ITEM_FOOD` | 19 | Food. |
| `DIKU_ITEM_MONEY` | 20 | Money. |
| `DIKU_ITEM_BOAT` | 22 | Boat/raft. |
| `DIKU_ITEM_CORPSE_NPC` | 23 | NPC corpse. |
| `DIKU_ITEM_CORPSE_PC` | 24 | Player corpse. |
| `DIKU_ITEM_FOUNTAIN` | 25 | Fountain. |
| `DIKU_ITEM_PILL` | 26 | Pill. |

## Canonical Sector Types

| Constant | Value | Description |
|----------|-------|-------------|
| `DIKU_SECT_INSIDE` | 0 | Indoor, no weather. |
| `DIKU_SECT_CITY` | 1 | City streets. |
| `DIKU_SECT_FIELD` | 2 | Open field. |
| `DIKU_SECT_FOREST` | 3 | Forest. |
| `DIKU_SECT_HILLS` | 4 | Hills. |
| `DIKU_SECT_MOUNTAIN` | 5 | Mountains. |
| `DIKU_SECT_WATER_SWIM` | 6 | Shallow water. |
| `DIKU_SECT_WATER_NOSWIM` | 7 | Deep water. |
| `DIKU_SECT_UNDERWATER` | 8 | Underwater. |
| `DIKU_SECT_AIR` | 9 | Air. |
| `DIKU_SECT_DESERT` | 10 | Desert. |

## Bitvector Decoder

```c
uint32_t flags = diku_decode_bitvector("ABZ");   /* bits 0, 1, 25 */
uint32_t flags = diku_decode_bitvector("aa");    /* bit 26 */
```

## Numeric Flag Parser

```c
uint32_t flags = diku_parse_numeric_flags("2|512");  /* 514 */
```

## See Also

- `include/diku/schema.h` — full API and implementation.
- `AGENTS.md` — agent-focused build and architecture notes.
- `INFERENCE.md` — raw research on fork-specific file formats.
