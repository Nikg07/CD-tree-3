// btree_map.h
#ifndef BTREE_MAP_H
#define BTREE_MAP_H

#include "map.h"

// Минимальная степень B-дерева (t). Рекомендуется t >= 2.
// Для тестов можно сделать параметр конфигурации.
#ifndef BTREE_T
#define BTREE_T 3
#endif

const MapOps* get_btree_map_ops(void);

// Визуализация B-дерева (специфична из-за множества ключей в узле)
void btree_draw(void* map, int x, int y, int dx);

#endif
