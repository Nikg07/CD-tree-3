// bst_map.h
#ifndef BST_MAP_H
#define BST_MAP_H

#include "map.h"

// Получить операции для BST-реализации
const MapOps* get_bst_map_ops(void);

// Дополнительная функция визуализации (использует Raylib)
// Принимает указатель на корень дерева и параметры отрисовки
// (только для внутреннего использования, не входит в MapOps)
void bst_draw(void* map, int x, int y, int dx);

#endif
