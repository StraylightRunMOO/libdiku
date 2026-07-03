
### 1. High-Level File Structure & Header Differences
The quickest way to differentiate formats is by checking the #AREA header or the global section layouts.
#### Vanilla Diku & Merc (1.x / 2.x)
 * **Header:** Extremely minimalist.
   ```text
   #AREA
   rotk.are~
   Romani~
   
   ```
   Or simply a single string containing the area name.
 * **Sections:** Strict sequential ordering using plural block names: #MOBILES, #OBJECTS, #ROOMS, #RESETS, #SHOPS, #SPECIALS.
 * **Termination:** The entire file ends with #0 or $.
#### ROM (Rivers of MUD - 2.3 / 2.4)
 * **Header:** ROM added structured metadata directly to the #AREA text line, including level ranges and authors enclosed in curly braces.
   ```text
   #AREA
   duran.are~
   Delta Luna~
   { 5 30} Author  Delta Luna~
   1200 1299
   
   ```
 * **Key Indicator:** The level range bracket ({ 5 30}) and the low/high VNUM range on the subsequent line are absolute signatures of ROM area files.
#### SMAUG
 * **Header:** Ditched purely positional lines in favor of pseudo-key-value pairs inside the #AREA block.
   ```text
   #AREA
   Name        The Dark Forest~
   Builders    Thoric~
   Vnums       1200 1250
   Duration    15
   Ranges      5 30 5 30
   
   ```
 * **Sections:** Often uses singular section tags (e.g., #MOBILE instead of #MOBILES, #ROOM instead of #ROOMS) or heavily extended tags like #PROGS, #REPAIRS.
#### CircleMUD
 * **File Distribution:** In native CircleMUD, areas are **not** combined into a single .are file. They are explicitly split into separate files: .wld (world/rooms), .mob (mobiles), .obj (objects), .zon (zone/resets), and .shp (shops).
 * **Termination:** Sections or individual files in CircleMUD typically terminate with $~ instead of #0.
### 2. Deep Dive: Section-Specific Syntax Variations
When the parser hits specific section headers, flag syntax and positional variables change drastically.
#### A. #MOBILES (Positional vs. Letter Bitvectors)
The mobile definitions highlight the core divergence between Merc, ROM, and SMAUG.
##### Merc 2.1 / 2.2 Format
Merc tracks values using purely space-delimited numeric fields.
```text
#1200
goblin sentry~
The goblin sentry~
A goblin sentry stands here, watching the path.
~
The goblin looks mean and green.
~
65 0 -200 S
5 20 0 1d6+5 1d6+1
100 200
8 8 1

```
 * **Line 6 (65 0 -200 S):** act_flags (65), affected_by (0), alignment (-200), and the hardcoded character 'S' (which stands for Simple/Standard Diku mobile structure).
##### ROM 2.4 Format
ROM added a significant number of combat and cosmetic attributes, altered the dice strings, and introduced alphabetic bitvectors.
```text
#1200
goblin sentry~
The goblin sentry~
A goblin sentry stands here, watching the path.
~
The goblin looks mean and green.
~
goblin~
A Z -200 0
5 5 2d6+60 2d6+60 1d6+1 punch
0 0 0 0
EFKR 0 0 0
stand stand male 100
0 0 S iron

```
 * **Line 6 (goblin~):** ROM inserts the **Race** string immediately after the description. If we parse a line here that ends in a tilde (~) rather than a sequence of numbers, it is **ROM 2.4+**.
 * **Line 7 (A Z -200 0):** Instead of integers like 65, ROM uses letters for flags (A = ACT_IS_NPC, Z = ACT_WARRIOR).
 * **Line 8 (5 5 2d6+60...):** ROM utilizes **three explicit sets of dice**: Hit Points (2d6+60), Mana (2d6+60), and Damage (1d6+1), followed by an explicit damage noun (punch, slash, bite). Merc only used two sets of dice and had no damage noun string.
 * **Line 10 (EFKR 0 0 0):** Defensive/Immunity bitvectors represented as letters.
 * **Line 12 (0 0 S iron):** Contains Size flag (S) and Material type (iron).
#### B. #OBJECTS
##### Merc Format
```text
#1201
sword short~
a short sword~
A small iron sword lies here.~
~
5 0 8193
0 2 6 3
5 50 0

```
 * **Line 5 (5 0 8193):** item_type (5 = weapon), extra_flags (0), wear_flags (8193). All are numbers.
 * **Line 6 (0 2 6 3):** The four core Diku value parameters (value0 to value3).
##### ROM 2.4 Format
```text
#1201
sword short~
a short sword~
A small iron sword lies here.~
iron~
weapon 0 AN
weapon 2 6 slash 0
5 50 0 P

```
 * **Line 5 (iron~):** ROM explicitly inserts the **Material** string right after the long description.
 * **Line 6 (weapon 0 AN):** The item type can be a string keyword (weapon), and flags are alphabetic (AN).
 * **Line 7 (weapon 2 6 slash 0):** Value fields can include lookup keywords (like weapon type and slash damage type).
#### C. #ROOMS
##### CircleMUD (.wld files)
CircleMUD changed the positioning of room flags and added a unique zone mapping integer.
```text
#1201
The Dark Pathway~
You are walking down a narrow, dark pathway.
~
12 1 3
D0
The path continues north.~
~
0 0 1202
S

```
 * **Line 4 (12 1 3):** zone_number (12), room_flags (1), sector_type (3). Pure Diku/Merc variants do not include the zone number on this line.
 * **Termination:** Each room block ends with a single letter S on its own line.
##### SMAUG Format
SMAUG abandons strict position-based integers for rooms, utilizing explicit tokens.
```text
#ROOM
Vnum         1201
Name         The Dark Pathway~
Desc         You are walking down a narrow, dark pathway.
~
Flags        dark indoors
Sector       forest
Exit         north ~ 0 1202
End

```

We may consider refactoring our parsing pipeline into an abstract base class with targeted overrides:

 1. **Introduce a Tokenizer layer:** Instead of reading line-by-line via fixed indexing (lines[5]), implement a tokenizer that streams tokens delimited by spaces, newlines, and tildes (~).
 2. **Alphabetic Bitvector Decoder:** Create a helper utility to decode string-encoded bitvectors. ROM uses standard bit offsets where A = 1, B = 2, C = 4, Z = 33554432, aa = 67108864 (lowercase doubles extend the bitvector to 64-bit bounds).
 3. **Dynamic Value Field Mapping:** Create an object mapping configuration based on the detected flavor. For instance, if the sniffer returns ROM24, the Object Parser must explicitly scan for the Material~ token right after the description string before attempting to parse numerical properties; if it's Merc, it should skip this step entirely.


Because these engines were written in C, the definitive lists of flags, bitvectors, and enumerations aren't usually in documentation files—they are hardcoded directly into the core header files of each server's source code. We should be able to find the server code with a quick seaech of github.

Below is a comprehensive reference of the essential flags and enumerations across the major forks, along with the specific legacy source files where you can find the complete lists.
### 1. Where to Find the Original Code Definitions
If you need to extract every single flag programmatically from legacy source trees, look into these exact files:
 * **Merc 2.1 / ROM 2.4:** Look at src/merc.h. (ROM kept the merc.h filename but vastly expanded the definitions).
 * **CircleMUD:** Look at src/structs.h (for flags/bitvectors) and src/spells.h (for affect vectors).
 * **SMAUG:** Look at src/mud.h.
### 2. Core Room Flags (room_flags)
Room flags dictate global behavior for environment spaces. In Merc/Circle, these are sequential integer bits. In ROM/SMAUG, they appear as alphabetical strings.
| Bit Value | ROM Letter | Flag Name | Description |
|---|---|---|---|
| 1 (1<<0) | A | **DARK** | Requires a light source to see. |
| 2 (1<<1) | B | **NO_MOB** | Monsters/NPCs cannot wander into this room. |
| 4 (1<<2) | C | **INDOORS** | Room is inside; roof blocks weather effects. |
| 8 (1<<3) | D | **PEACEFUL** | Combat is disabled (common in CircleMUD). |
| 16 (1<<4) | E | **STEAL** | Stealing is allowed (or specifically handled). |
| 32 (1<<5) | F | **NO_MAGIC** | Magic spells cannot be cast here. |
| 64 (1<<6) | G | **TUNNEL** | Restricts the room to a limited capacity of players. |
| 128 (1<<7) | H | **PRIVATE** | Room holds a maximum of 2 characters. |
| 256 (1<<8) | I | **GODS_ONLY** | Restricted to administrative staff/Immortals. |
### 3. Mobile Act Flags (act_flags)
These flags dictate the baseline AI behaviors of NPCs.
```text
Merc/Vanilla Bit values:
1       = ACT_IS_NPC (Always true for mobiles in area files)
2       = ACT_SENTINEL (Mobile stays in its spawning room; won't wander)
4       = ACT_SCAVENGER (Picks up items left on the ground)
16      = ACT_AGGRESSIVE (Attacks players on sight)
32      = ACT_STAY_AREA (Won't wander outside its home zone)
64      = ACT_WIMPY (Flees combat when low on health)
128     = ACT_PET (Bought from a pet shop)
256     = ACT_TRAIN (Can train player attributes)
512     = ACT_PRACTICE (Can practice player skills)

```
#### ROM 2.4 Alphabetic Mapping for Act Flags:
ROM mapped these to letters and expanded them heavily:
 * A : ACT_IS_NPC
 * B : ACT_SENTINEL
 * C : ACT_SCAVENGER
 * E : ACT_AGGRESSIVE
 * F : ACT_STAY_AREA
 * G : ACT_WIMPY
 * H : ACT_PET
 * I : ACT_TRAIN
 * J : ACT_PRACTICE
 * U : ACT_AGGRESSIVE_EVIL (Attacks evil alignment characters)
 * V : ACT_AGGRESSIVE_GOOD (Attacks good alignment characters)
### 4. Item Types (item_type)
Item types determine how the engine parses the four trailing value fields (value0 to value3) on an object. In Merc and Circle, these are integers. In ROM and SMAUG, they can be strings or integers.
| Integer ID | Constant Name | Value Array Signatures (What v0 through v3 mean) |
|---|---|---|
| **1** | ITEM_LIGHT | v0: parsing flag, v2: hours of light remaining (-1 = infinite). |
| **2** | ITEM_SCROLL | v0: spell level, v1: spell 1, v2: spell 2, v3: spell 3. |
| **3** | ITEM_WAND | v0: spell level, v1: max charges, v2: current charges, v3: spell ID. |
| **4** | ITEM_STAFF | v0: spell level, v1: max charges, v2: current charges, v3: spell ID. |
| **5** | ITEM_WEAPON | v0: condition/type, v1: min damage dice, v2: max damage dice, v3: damage type ID. |
| **9** | ITEM_ARMOR | v0: AC pierce, v1: AC bash, v2: AC slash, v3: AC exotic (ROM scales this). |
| **10** | ITEM_POTION | v0: spell level, v1: spell 1, v2: spell 2, v3: spell 3. |
| **11** | ITEM_CONTAINER | v0: weight capacity, v1: flags (lockable/closeable), v2: key VNUM, v3: weight multiplier. |
| **15** | ITEM_DRINK_CON | v0: max capacity, v1: current quantity, v2: liquid type ID, v3: poisoned flag (0/1). |
| **18** | ITEM_MONEY | v0: gold coins value, v1: silver coins value (ROM added silver). |
### 5. Object Wear Flags (wear_flags)
Wear flags use a bitmask structure to identify where a character can equip an item. An item can have multiple flags combined (e.g., ITEM_TAKE + ITEM_WEAR_FINGER).
```text
Value       ROM Letter    Constant Name
1           A             ITEM_TAKE (Must be set for an item to be picked up)
2           B             ITEM_WEAR_FINGER
4           C             ITEM_WEAR_NECK
8           D             ITEM_WEAR_BODY
16          E             ITEM_WEAR_HEAD
32          F             ITEM_WEAR_LEGS
64          G             ITEM_WEAR_FEET
128         H             ITEM_WEAR_HANDS
256         I             ITEM_WEAR_ARMS
512         J             ITEM_WEAR_SHIELD
1024        K             ITEM_WEAR_ABOUT
2048        L             ITEM_WEAR_WAIST
4096        M             ITEM_WEAR_WRIST
8192        N             ITEM_WIELD (Weapon slots)
16384       O             ITEM_HOLD (Held items/lights)

```
> **Parsing Tip for libdiku:** If you find a number like 8193 in a Merc or early Circle file under wear flags, you decode it by checking bitwise operations: 8193 & 1 (Take) and 8193 & 8192 (Wield). If it's ROM, you'll see the literal string AN.
> 
### 6. Sector Types (sector_type)
Found in the #ROOMS configurations, sector types determine movement point costs and wilderness handling. They are almost universally sequential integers starting at 0.
 * **0**: SECT_INSIDE (Indoor spaces, no weather)
 * **1**: SECT_CITY (Smooth walking, low movement cost)
 * **2**: SECT_FIELD (Open grassy plains)
 * **3**: SECT_FOREST (Tree coverage, hidden exits)
 * **4**: SECT_HILLS (Rough terrain)
 * **5**: SECT_MOUNTAIN (High movement cost, dangerous)
 * **6**: SECT_WATER_SWIM (Water traversable on foot/boat)
 * **7**: SECT_WATER_NOSWIM (Deep water requiring a boat)
 * **8**: SECT_UNUSED / SECT_UNDERWATER
 * **9**: SECT_AIR (Requires flight bitvector)
 * **10**: SECT_DESERT (High heat, fast dehydration)



