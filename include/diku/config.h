#ifndef DIKU_CONFIG_H
#define DIKU_CONFIG_H

#define DIKU_MAX_EXITS      12
#define DIKU_VNUM_HASH_BITS 12
#define DIKU_VNUM_HASH_SIZE (1 << DIKU_VNUM_HASH_BITS)
#define DIKU_VNUM_HASH_MASK (DIKU_VNUM_HASH_SIZE - 1)

#define DIR_NORTH       0
#define DIR_EAST        1
#define DIR_SOUTH       2
#define DIR_WEST        3
#define DIR_UP          4
#define DIR_DOWN        5
#define DIR_NORTHEAST   6
#define DIR_NORTHWEST   7
#define DIR_SOUTHEAST   8
#define DIR_SOUTHWEST   9
#define DIR_IN          10
#define DIR_OUT         11

static const int dir_offset[12][3] = {
    { 0,  1,  0},  /* North:      +Y */
    { 1,  0,  0},  /* East:       +X */
    { 0, -1,  0},  /* South:      -Y */
    {-1,  0,  0},  /* West:       -X */
    { 0,  0,  1},  /* Up:         +Z */
    { 0,  0, -1},  /* Down:       -Z */
    { 1,  1,  0},  /* Northeast:  +X +Y */
    {-1,  1,  0},  /* Northwest:  -X +Y */
    { 1, -1,  0},  /* Southeast:  +X -Y */
    {-1, -1,  0},  /* Southwest:  -X -Y */
    { 0,  0,  0},  /* In:          0  0 */
    { 0,  0,  0},  /* Out:         0  0 */
};

#endif /* DIKU_CONFIG_H */
