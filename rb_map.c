#define _CRT_SECURE_NO_WARNINGS
#include "rb_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef VISUALIZATION
#include "raylib.h"
#define MAX_PATH_VIS 100
extern KeyType g_search_path[MAX_PATH_VIS];
extern int g_search_path_len;
extern bool g_search_found;
#endif

typedef enum { NODE_RED, NODE_BLACK } NodeColor;

typedef struct RBNode {
    KeyType key;
    ValueType value;
    NodeColor color;
    struct RBNode* left;
    struct RBNode* right;
    struct RBNode* parent;
} RBNode;

typedef struct {
    RBNode* root;
    size_t size;
} RBMap;

static RBNode* rb_minimum(RBNode* node) {
    while (node && node->left) node = node->left;
    return node;
}

static RBNode* rb_node_create(KeyType key, ValueType value) {
    RBNode* node = malloc(sizeof(RBNode));
    if (!node) return NULL;
    node->key = key;
    node->value = (value) ? strdup(value) : NULL;
    node->color = NODE_RED;
    node->left = node->right = node->parent = NULL;
    return node;
}

static void rb_node_destroy(RBNode* node) {
    if (!node) return;
    rb_node_destroy(node->left);
    rb_node_destroy(node->right);
    map_free_value(node->value);
    free(node);
}

// ======================= Повороты =======================

static void rb_rotate_left(RBMap* map, RBNode* x) {
    RBNode* y = x->right;
    x->right = y->left;
    if (y->left) y->left->parent = x;
    y->parent = x->parent;
    if (!x->parent)
        map->root = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;
    y->left = x;
    x->parent = y;
}

static void rb_rotate_right(RBMap* map, RBNode* y) {
    RBNode* x = y->left;
    y->left = x->right;
    if (x->right) x->right->parent = y;
    x->parent = y->parent;
    if (!y->parent)
        map->root = x;
    else if (y == y->parent->right)
        y->parent->right = x;
    else
        y->parent->left = x;
    x->right = y;
    y->parent = x;
}

// ======================= Фиксация вставки =======================

static void rb_fix_insert(RBMap* map, RBNode* node) {
    while (node->parent && node->parent->color == NODE_RED) {
        RBNode* parent = node->parent;
        RBNode* grandparent = parent->parent;
        RBNode* uncle = (parent == grandparent->left) ? grandparent->right : grandparent->left;

        if (uncle && uncle->color == NODE_RED) {
            parent->color = NODE_BLACK;
            uncle->color = NODE_BLACK;
            grandparent->color = NODE_RED;
            node = grandparent;
        } else {
            if (node == parent->right && parent == grandparent->left) {
                rb_rotate_left(map, parent);
                node = parent;
                parent = node->parent;
            } else if (node == parent->left && parent == grandparent->right) {
                rb_rotate_right(map, parent);
                node = parent;
                parent = node->parent;
            }

            parent->color = NODE_BLACK;
            grandparent->color = NODE_RED;

            if (node == parent->left)
                rb_rotate_right(map, grandparent);
            else
                rb_rotate_left(map, grandparent);
        }
    }
    if (map->root) map->root->color = NODE_BLACK;
}

// ======================= Transplant =======================

static void rb_transplant(RBMap* map, RBNode* u, RBNode* v) {
    if (!u->parent)
        map->root = v;
    else if (u == u->parent->left)
        u->parent->left = v;
    else
        u->parent->right = v;

    if (v) v->parent = u->parent;
}

// ======================= Фиксация удаления =======================

static void rb_fix_delete(RBMap* map, RBNode* x) {
    while (x != map->root && (x == NULL || x->color == NODE_BLACK)) {
        if (x == NULL) break;

        if (x == x->parent->left) {
            RBNode* sibling = x->parent->right;

            if (sibling && sibling->color == NODE_RED) {
                sibling->color = NODE_BLACK;
                x->parent->color = NODE_RED;
                rb_rotate_left(map, x->parent);
                sibling = x->parent->right;
            }

            if ((!sibling->left || sibling->left->color == NODE_BLACK) &&
                (!sibling->right || sibling->right->color == NODE_BLACK)) {
                sibling->color = NODE_RED;
                x = x->parent;
            } else {
                if (!sibling->right || sibling->right->color == NODE_BLACK) {
                    if (sibling->left) sibling->left->color = NODE_BLACK;
                    sibling->color = NODE_RED;
                    rb_rotate_right(map, sibling);
                    sibling = x->parent->right;
                }
                sibling->color = x->parent->color;
                x->parent->color = NODE_BLACK;
                if (sibling->right) sibling->right->color = NODE_BLACK;
                rb_rotate_left(map, x->parent);
                x = map->root;
            }
        } else {
            // Симметричный случай (x — правый ребёнок)
            RBNode* sibling = x->parent->left;

            if (sibling && sibling->color == NODE_RED) {
                sibling->color = NODE_BLACK;
                x->parent->color = NODE_RED;
                rb_rotate_right(map, x->parent);
                sibling = x->parent->left;
            }

            if ((!sibling->left || sibling->left->color == NODE_BLACK) &&
                (!sibling->right || sibling->right->color == NODE_BLACK)) {
                sibling->color = NODE_RED;
                x = x->parent;
            } else {
                if (!sibling->left || sibling->left->color == NODE_BLACK) {
                    if (sibling->right) sibling->right->color = NODE_BLACK;
                    sibling->color = NODE_RED;
                    rb_rotate_left(map, sibling);
                    sibling = x->parent->left;
                }
                sibling->color = x->parent->color;
                x->parent->color = NODE_BLACK;
                if (sibling->left) sibling->left->color = NODE_BLACK;
                rb_rotate_right(map, x->parent);
                x = map->root;
            }
        }
    }
    if (x) x->color = NODE_BLACK;
}

static void* rb_create(void) {
    RBMap* map = malloc(sizeof(RBMap));
    if (!map) return NULL;
    map->root = NULL;
    map->size = 0;
    return map;
}

static void rb_destroy(void* map_ptr) {
    RBMap* map = (RBMap*)map_ptr;
    if (map) {
        rb_node_destroy(map->root);
        free(map);
    }
}

static bool rb_insert(void* map_ptr, KeyType key, ValueType value) {
    RBMap* map = (RBMap*)map_ptr;
    RBNode* new_node = rb_node_create(key, value);
    if (!new_node) return false;

    RBNode* parent = NULL;
    RBNode* current = map->root;

    while (current) {
        parent = current;
        if (key < current->key)
            current = current->left;
        else if (key > current->key)
            current = current->right;
        else {
            map_free_value(current->value);
            current->value = (value) ? strdup(value) : NULL;
            free(new_node);
            return true;
        }
    }

    new_node->parent = parent;
    if (!parent)
        map->root = new_node;
    else if (key < parent->key)
        parent->left = new_node;
    else
        parent->right = new_node;

    rb_fix_insert(map, new_node);
    map->size++;
    return true;
}

static bool rb_erase(void* map_ptr, KeyType key) {
    RBMap* map = (RBMap*)map_ptr;
    if (!map->root) return false;

    RBNode* z = map->root;
    while (z) {
        if (key < z->key) z = z->left;
        else if (key > z->key) z = z->right;
        else break;
    }
    if (!z) return false;

    RBNode* y = z;
    RBNode* x = NULL;
    NodeColor original_color = y->color;

    if (!z->left) {
        x = z->right;
        rb_transplant(map, z, z->right);
    } else if (!z->right) {
        x = z->left;
        rb_transplant(map, z, z->left);
    } else {
        y = rb_minimum(z->right);
        original_color = y->color;
        x = y->right;

        if (y->parent == z) {
            if (x) x->parent = y;
        } else {
            rb_transplant(map, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }

        rb_transplant(map, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }

    map_free_value(z->value);
    free(z);

    if (original_color == NODE_BLACK && x != NULL)
        rb_fix_delete(map, x);

    if (map->root) map->root->color = NODE_BLACK;

    map->size--;
    return true;
}

static ValueType rb_find(void* map_ptr, KeyType key) {
    RBMap* map = (RBMap*)map_ptr;
    RBNode* node = map->root;
    while (node) {
        if (key < node->key) node = node->left;
        else if (key > node->key) node = node->right;
        else return node->value;
    }
    return NULL;
}

static void rb_traverse_node(RBNode* node, void (*visit)(KeyType, ValueType)) {
    if (!node) return;
    rb_traverse_node(node->left, visit);
    visit(node->key, node->value);
    rb_traverse_node(node->right, visit);
}

static void rb_traverse(void* map_ptr, void (*visit)(KeyType, ValueType)) {
    RBMap* map = (RBMap*)map_ptr;
    rb_traverse_node(map->root, visit);
}

static size_t rb_height_node(RBNode* node) {
    if (!node) return 0;
    size_t lh = rb_height_node(node->left);
    size_t rh = rb_height_node(node->right);
    return 1 + (lh > rh ? lh : rh);
}

static size_t rb_height(void* map_ptr) {
    RBMap* map = (RBMap*)map_ptr;
    return rb_height_node(map->root);
}

static const MapOps rb_ops = {
    .create   = rb_create,
    .destroy  = rb_destroy,
    .insert   = rb_insert,
    .find     = rb_find,
    .erase    = rb_erase,
    .traverse = rb_traverse,
    .height   = rb_height
};

// ======================= Поиск с путём и отрисовка =======================

static bool rb_find_with_path(void* map_ptr, KeyType key, KeyType* path_keys, int* path_len, int max_path) {
    RBMap* map = (RBMap*)map_ptr;
    RBNode* node = map->root;
    *path_len = 0;
    while (node) {
        if (*path_len < max_path)
            path_keys[(*path_len)++] = node->key;
        if (key < node->key)
            node = node->left;
        else if (key > node->key)
            node = node->right;
        else
            return true;
    }
    return false;
}

#ifdef VISUALIZATION
static bool is_key_in_path(KeyType key, bool* is_found) {
    for (int i = 0; i < g_search_path_len; i++) {
        if (g_search_path[i] == key) {
            *is_found = (i == g_search_path_len - 1) && g_search_found;
            return true;
        }
    }
    *is_found = false;
    return false;
}

static void rb_draw_node(RBNode* node, int x, int y, int dx) {
    if (!node) return;

    Color node_color = (node->color == NODE_RED) ? RED : BLACK;
    Color text_color = (node->color == NODE_RED) ? BLACK : WHITE;
    Color border_color = (node->color == NODE_RED) ? BLACK : RED;;

    bool is_found = false;
    if (is_key_in_path(node->key, &is_found)) {
        if (is_found) {
            node_color = LIME;
            text_color = BLACK;
            border_color = DARKGREEN;
        } else {
            node_color = GRAY;
            text_color = WHITE;
            border_color = DARKGRAY;
        }
    }

    DrawCircle(x, y, 18, node_color);
    DrawCircleLines(x, y, 18, border_color);

    char key_str[16];
    sprintf(key_str, "%d", node->key);
    int text_width = MeasureText(key_str, 18);
    DrawText(key_str, x - text_width / 2, y - 9, 18, text_color);

    if (node->left) {
        int cx = x - dx;
        int cy = y + 60;
        DrawLine(x, y + 18, cx, cy - 18, DARKGRAY);
        rb_draw_node(node->left, cx, cy, (int)(dx / 1.5f));
    }
    if (node->right) {
        int cx = x + dx;
        int cy = y + 60;
        DrawLine(x, y + 18, cx, cy - 18, DARKGRAY);
        rb_draw_node(node->right, cx, cy, (int)(dx / 1.5f));
    }
}

static void rb_draw(void* map_ptr, int x, int y, int dx) {
    RBMap* map = (RBMap*)map_ptr;
    if (map->root)
        rb_draw_node(map->root, x, y, dx);
}
#else
static void rb_draw(void* map_ptr, int x, int y, int dx) {
    (void)map_ptr; (void)x; (void)y; (void)dx;
}
#endif

const TreeImpl rb_tree_impl = {
    .ops            = &rb_ops,
    .draw           = rb_draw,
    .find_with_path = rb_find_with_path
};

const TreeImpl* get_rb_tree_impl(void) {
    return &rb_tree_impl;
}
