#define _CRT_SECURE_NO_WARNINGS
#include "map.h"
#include <stdlib.h>

void map_free_value(ValueType val) {
    free(val);
}