#ifndef DIKU_PARSER_H
#define DIKU_PARSER_H

#include "diku/config.h"
#include "diku/types.h"
#include "diku/arena.h"
#include "diku/context.h"
#include "diku/lexer.h"
#include "diku/util.h"
#include "diku/find.h"
#include "diku/resets.h"
#include "diku/sections.h"
#include "diku/format.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Arena helpers — declared and implemented in diku/arena.h */

/* Progress & context — declared in diku/context.h */

/* Lexer API — declared and implemented in diku/lexer.h */

/* Section parsers — declared and implemented in diku/sections.h */

area_t *diku_parse_file(const char *filename);
area_t *diku_parse_lexer(diku_lexer_t *lex, const char *filename);
area_t *diku_parse_package(const char *base_path);
area_t *diku_parse_package_files(const char *wld, const char *mob, const char *obj, const char *zon);
area_t *diku_load_folder_are(const char *folder_path);
area_t *diku_load_folder_packages(const char *folder_path);
/* diku_parse_resets — declared and implemented in diku/resets.h */

/* ------------------------------------------------------------------ */
/* Graph resolution                                                   */
/* ------------------------------------------------------------------ */
void diku_resolve_graph(area_t **areas, int area_count);
void diku_resolve_graph_global(area_t *areas);
/* diku_find_room*, diku_find_mobile, diku_find_item — in diku/find.h */

/* ------------------------------------------------------------------ */
/* Coordinate assignment                                              */
/* ------------------------------------------------------------------ */
room_t *diku_find_central_room(area_t *area);
void diku_assign_coordinates(area_t *area, room_t *root);
void diku_assign_coordinates_all(area_t *areas);
void diku_assign_coordinates_multi(area_t *areas);
room_t *diku_room_at_coord(area_t *area, int x, int y, int z);
bool diku_coord_occupied(area_t *area, int x, int y, int z, room_t *exclude);
void diku_center_coordinates(area_t *area);
void diku_get_coord_bounds(area_t *area, int *min_x, int *max_x, int *min_y, int *max_y, int *min_z, int *max_z);

/* ------------------------------------------------------------------ */
/* Utility & debug                                                    */
/* ------------------------------------------------------------------ */
void diku_free_area(area_t *area);
void diku_free_all_areas(area_t *areas);
void diku_print_area_info(const area_t *area);
void diku_print_room(const room_t *room);
void diku_print_mobile(const mobile_t *mob);
void diku_print_item(const item_t *item);
void diku_print_graph(const area_t *area);
void diku_print_coordinates(area_t *area);
/* diku_dir_name, diku_dir_name_short, diku_reverse_dir, diku_has_exit,
   diku_get_exit_vnum, diku_count_exits, diku_sector_name, diku_item_type_name,
   diku_format_name — defined as static inline in diku/util.h */
int diku_graph_diameter(area_t *area);
int diku_check_exit_symmetry(const area_t *area, int *out_total, int *out_asymmetric);
void diku_print_exit_symmetry(const area_t *area);

/* ------------------------------------------------------------------ */
/* Fork detection                                                     */
/* ------------------------------------------------------------------ */
diku_format_t diku_detect_format(const area_t *area);
/* diku_format_name, diku_sector_name, diku_item_type_name — in diku/util.h */

#ifdef __cplusplus
}
#endif
#endif /* DIKU_PARSER_H */
