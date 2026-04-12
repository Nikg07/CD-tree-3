#define _CRT_SECURE_NO_WARNINGS
#include "bst_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef VISUALIZATION
#include "raylib.h"
#define MAX_PATH_VIS 100
extern KeyType g_search_path[MAX_PATH_VIS];
extern int g_search_path_len;
extern bool g_search_found;  // найден ли ключ
#endif

typedef struct BSTNode {
    KeyType key;
    ValueType value;
    struct BSTNode* left;
    struct BSTNode* right;
} BSTNode;

typedef struct {
    BSTNode* root;
    size_t size;
} BSTMap;

static ValueType copy_value(ValueType val) {
    if (!val) return NULL;
    char* new_val = malloc(strlen(val) + 1);
    if (new_val) strcpy(new_val, val);
    return new_val;
}

static BSTNode* bst_node_create(KeyType key, ValueType value) {
    BSTNode* node = malloc(sizeof(BSTNode));
    if (!node) return NULL;
    node->key = key;
    node->value = copy_value(value);
    node->left = node->right = NULL;
    return node;
}

static void bst_node_destroy(BSTNode* node) {
    if (!node) return;
    bst_node_destroy(node->left);
    bst_node_destroy(node->right);
    map_free_value(node->value);
    free(node);
}

static BSTNode* bst_insert_node(BSTNode* node, KeyType key, ValueType value, bool* replaced) {
    if (!node) {
        *replaced = false;
        return bst_node_create(key, value);
    }
    if (key < node->key) {
        node->left = bst_insert_node(node->left, key, value, replaced);
    }
    else if (key > node->key) {
        node->right = bst_insert_node(node->right, key, value, replaced);
    }
    else {
        map_free_value(node->value);
        node->value = copy_value(value);
        *replaced = true;
    }
    return node;
}

static BSTNode* bst_find_node(BSTNode* node, KeyType key) {
    while (node) {
        if (key < node->key) node = node->left;
        else if (key > node->key) node = node->right;
        else return node;
    }
    return NULL;
}

static BSTNode* bst_find_min(BSTNode* node) {
    while (node && node->left) node = node->left;
    return node;
}

static BSTNode* bst_erase_node(BSTNode* node, KeyType key, bool* erased) {
    if (!node) {
        *erased = false;
        return NULL;
    }
    if (key < node->key) {
        node->left = bst_erase_node(node->left, key, erased);
    }
    else if (key > node->key) {
        node->right = bst_erase_node(node->right, key, erased);
    }
    else {
        *erased = true;
        if (!node->left) {
            BSTNode* right = node->right;
            map_free_value(node->value);
            free(node);
            return right;
        }
        else if (!node->right) {
            BSTNode* left = node->left;
            map_free_value(node->value);
            free(node);
            return left;
        }
        else {
            BSTNode* min_right = bst_find_min(node->right);
            node->key = min_right->key;
            map_free_value(node->value);
            node->value = copy_value(min_right->value);
            node->right = bst_erase_node(node->right, min_right->key, &(bool){false});
        }
    }
    return node;
}

static void bst_traverse_node(BSTNode* node, void (*visit)(KeyType, ValueType)) {
    if (!node) return;
    bst_traverse_node(node->left, visit);
    visit(node->key, node->value);
    bst_traverse_node(node->right, visit);
}

static size_t bst_height_node(BSTNode* node) {
    if (!node) return 0;
    size_t left_h = bst_height_node(node->left);
    size_t right_h = bst_height_node(node->right);
    return 1 + (left_h > right_h ? left_h : right_h);
}

// ----- MapOps -----
static void* bst_create(void) {
    BSTMap* map = malloc(sizeof(BSTMap));
    if (!map) return NULL;
    map->root = NULL;
    map->size = 0;
    return map;
}

static void bst_destroy(void* map_ptr) {
    BSTMap* map = (BSTMap*)map_ptr;
    if (!map) return;
    bst_node_destroy(map->root);
    free(map);
}

static bool bst_insert(void* map_ptr, KeyType key, ValueType value) {
    BSTMap* map = (BSTMap*)map_ptr;
    bool replaced = false;
    map->root = bst_insert_node(map->root, key, value, &replaced);
    if (!replaced) map->size++;
    return true;
}

static ValueType bst_find(void* map_ptr, KeyType key) {
    BSTMap* map = (BSTMap*)map_ptr;
    BSTNode* node = bst_find_node(map->root, key);
    return node ? node->value : NULL;
}

static bool bst_erase(void* map_ptr, KeyType key) {
    BSTMap* map = (BSTMap*)map_ptr;
    bool erased = false;
    map->root = bst_erase_node(map->root, key, &erased);
    if (erased) map->size--;
    return erased;
}

static void bst_traverse(void* map_ptr, void (*visit)(KeyType, ValueType)) {
    BSTMap* map = (BSTMap*)map_ptr;
    bst_traverse_node(map->root, visit);
}

static size_t bst_height(void* map_ptr) {
    BSTMap* map = (BSTMap*)map_ptr;
    return bst_height_node(map->root);
}

static const MapOps bst_ops = {
    .create = bst_create,
    .destroy = bst_destroy,
    .insert = bst_insert,
    .find = bst_find,
    .erase = bst_erase,
    .traverse = bst_traverse,
    .height = bst_height
};

// ----- TreeImpl -----
static bool bst_find_with_path(void* map_ptr, KeyType key, KeyType* path_keys, int* path_len, int max_path) {
    BSTMap* map = (BSTMap*)map_ptr;
    BSTNode* node = map->root;
    *path_len = 0;
    while (node) {
        if (*path_len < max_path) {
            path_keys[(*path_len)++] = node->key;
        }
        if (key < node->key) {
            node = node->left;
        }
        else if (key > node->key) {
            node = node->right;
        }
        else {
            return true;
        }
    }
    return false;
}

#ifdef VISUALIZATION
static bool is_key_in_path(KeyType key, bool* is_found) {
    for (int i = 0; i < g_search_path_len; i++) {
        if (g_search_path[i] == key) {
            // Проверяем, является ли этот ключ последним в пути (найденным)
            *is_found = (i == g_search_path_len - 1) && g_search_found;
            return true;
        }
    }
    *is_found = false;
    return false;
}

static void bst_draw_node(BSTNode* node, int x, int y, int dx) {
    if (!node) return;
    Color node_color = LIGHTGRAY;
    Color border_color = DARKBLUE;

    bool is_found = false;
    if (is_key_in_path(node->key, &is_found)) {
        if (is_found) {
            node_color = LIME;       // найденный узел
            border_color = DARKGREEN;
        }
        else {
            node_color = (Color){ 255, 136, 136, 255 };  // слабо-красный (путь)
            border_color = RED;
        }
    }

    DrawCircle(x, y, 18, node_color);
    DrawCircleLines(x, y, 18, border_color);
    char key_str[16];
    sprintf(key_str, "%d", node->key);
    int text_width = MeasureText(key_str, 18);
    DrawText(key_str, x - text_width / 2, y - 9, 18, BLACK);

    if (node->left) {
        int cx = x - dx;
        int cy = y + 60;
        DrawLine(x, y + 18, cx, cy - 18, DARKGRAY);
        bst_draw_node(node->left, cx, cy, (int)(dx / 1.5f));
    }
    if (node->right) {
        int cx = x + dx;
        int cy = y + 60;
        DrawLine(x, y + 18, cx, cy - 18, DARKGRAY);
        bst_draw_node(node->right, cx, cy, (int)(dx / 1.5f));
    }
}

static void bst_draw(void* map_ptr, int x, int y, int dx) {
    BSTMap* map = (BSTMap*)map_ptr;
    bst_draw_node(map->root, x, y, dx);
}
#else
static void bst_draw(void* map_ptr, int x, int y, int dx) {
    (void)map_ptr; (void)x; (void)y; (void)dx;
}
#endif

const TreeImpl bst_tree_impl = {
    .ops = &bst_ops,
    .draw = bst_draw,
    .find_with_path = bst_find_with_path
};

const TreeImpl* get_bst_tree_impl(void) {
    return &bst_tree_impl;
}
