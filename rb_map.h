// rb_map.h
#ifndef RB_MAP_H
#define RB_MAP_H

#include "map.h"

const MapOps* get_rb_map_ops(void);

void rb_draw(void* map, int x, int y, int dx);

#endif
