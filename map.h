// map.h
#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include <stddef.h>

typedef int KeyType;
typedef char* ValueType;

// Функция для освобождения значения (используется внутри map)
void map_free_value(ValueType val);

// Структура с операциями для конкретной реализации map
typedef struct MapOps {
    void* (*create)(void);
    void  (*destroy)(void* map);
    bool  (*insert)(void* map, KeyType key, ValueType value);
    ValueType(*find)(void* map, KeyType key);
    bool  (*erase)(void* map, KeyType key);
    void  (*traverse)(void* map, void (*visit)(KeyType, ValueType));
    size_t(*height)(void* map);
} MapOps;

#endif
