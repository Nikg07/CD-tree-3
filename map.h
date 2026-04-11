#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include <stddef.h>

typedef int KeyType;
typedef char* ValueType;

void map_free_value(ValueType val);

typedef struct MapOps {
    void* (*create)(void);
    void  (*destroy)(void* map);
    bool  (*insert)(void* map, KeyType key, ValueType value);
    ValueType(*find)(void* map, KeyType key);
    bool  (*erase)(void* map, KeyType key);
    void  (*traverse)(void* map, void (*visit)(KeyType, ValueType));
    size_t(*height)(void* map);
} MapOps;

typedef struct TreeImpl {
    const MapOps* ops;
    void (*draw)(void* map, int x, int y, int dx);
    bool (*find_with_path)(void* map, KeyType key, KeyType* path_keys, int* path_len, int max_path);
} TreeImpl;

#endif