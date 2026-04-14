#define _CRT_SECURE_NO_WARNINGS
#include "btree_map.h"
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

// Параметры B-дерева
#define T 3                    // минимальная степень (t = 3)
#define MAX_KEYS (2 * T - 1)   // максимум ключей в узле = 5
#define MIN_KEYS (T - 1)       // минимум ключей в узле = 2

typedef struct BTreeNode {
    int n;                              // количество ключей
    KeyType keys[MAX_KEYS];             // массив ключей
    ValueType values[MAX_KEYS];         // массив значений
    struct BTreeNode* children[MAX_KEYS + 1]; // дочерние узлы
    bool leaf;                          // является ли листом
} BTreeNode;

typedef struct {
    BTreeNode* root;
    size_t size;
} BTreeMap;

static ValueType copy_value(ValueType val) {
    if (!val) return NULL;
    char* new_val = malloc(strlen(val) + 1);
    if (new_val) strcpy(new_val, val);
    return new_val;
}

static BTreeNode* btree_node_create(bool leaf) {
    BTreeNode* node = malloc(sizeof(BTreeNode));
    node->n = 0;
    node->leaf = leaf;
    for (int i = 0; i <= MAX_KEYS; i++) {
        node->children[i] = NULL;
    }
    return node;
}

static void btree_node_destroy(BTreeNode* node) {
    if (!node) return;
    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++) {
            btree_node_destroy(node->children[i]);
        }
    }
    for (int i = 0; i < node->n; i++) {
        map_free_value(node->values[i]);
    }
    free(node);
}

// Поиск ключа в узле
static int btree_find_key_index(BTreeNode* node, KeyType key) {
    int idx = 0;
    while (idx < node->n && key > node->keys[idx]) idx++;
    return idx;
}

// Поиск в B-дереве
static ValueType btree_search_node(BTreeNode* node, KeyType key) {
    if (!node) return NULL;
    int i = 0;
    while (i < node->n && key > node->keys[i]) i++;
    if (i < node->n && key == node->keys[i]) return node->values[i];
    if (node->leaf) return NULL;
    return btree_search_node(node->children[i], key);
}

// Разбиение полного дочернего узла
static void btree_split_child(BTreeNode* parent, int i, BTreeNode* child) {
    // child имеет MAX_KEYS ключей (5), нужно разбить на два узла
    // Левый узел: keys[0..MIN_KEYS-1] (2 ключа)
    // Правый узел: keys[MIN_KEYS+1..MAX_KEYS-1] (2 ключа)
    // Средний ключ keys[MIN_KEYS] поднимается в родителя

    BTreeNode* new_node = btree_node_create(child->leaf);
    new_node->n = MIN_KEYS;  // MIN_KEYS = T-1 = 2

    // Копируем ПРАВУЮ половину ключей и значений в new_node
    for (int j = 0; j < MIN_KEYS; j++) {
        new_node->keys[j] = child->keys[j + T];
        new_node->values[j] = child->values[j + T];
    }

    // Копируем дочерние указатели для new_node (если не лист)
    if (!child->leaf) {
        for (int j = 0; j <= MIN_KEYS; j++) {
            new_node->children[j] = child->children[j + T];
        }
    }

    // Уменьшаем количество ключей в child
    child->n = MIN_KEYS;

    // Сдвигаем дочерние указатели родителя вправо
    for (int j = parent->n; j >= i + 1; j--) {
        parent->children[j + 1] = parent->children[j];
    }
    parent->children[i + 1] = new_node;

    // Сдвигаем ключи родителя вправо
    for (int j = parent->n - 1; j >= i; j--) {
        parent->keys[j + 1] = parent->keys[j];
        parent->values[j + 1] = parent->values[j];
    }

    // Поднимаем СРЕДНИЙ ключ в родителя
    parent->keys[i] = child->keys[MIN_KEYS];
    parent->values[i] = child->values[MIN_KEYS];
    parent->n++;
}

// Вставка в неполный узел
static void btree_insert_nonfull(BTreeNode* node, KeyType key, ValueType value) {
    int i = node->n - 1;

    if (node->leaf) {
        // Поиск позиции для вставки
        while (i >= 0 && key < node->keys[i]) {
            node->keys[i + 1] = node->keys[i];
            node->values[i + 1] = node->values[i];
            i--;
        }

        // Проверка на существующий ключ
        if (i >= 0 && key == node->keys[i]) {
            map_free_value(node->values[i]);
            node->values[i] = copy_value(value);
        }
        else {
            i++;
            node->keys[i] = key;
            node->values[i] = copy_value(value);
            node->n++;
        }
    }
    else {
        // Найти дочерний узел для вставки
        while (i >= 0 && key < node->keys[i]) i--;
        i++;

        // Если дочерний узел полон, разбить его
        if (node->children[i]->n == MAX_KEYS) {
            btree_split_child(node, i, node->children[i]);
            if (key > node->keys[i]) i++;
        }
        btree_insert_nonfull(node->children[i], key, value);
    }
}

// Вставка в B-дерево
static bool btree_insert(BTreeMap* map, KeyType key, ValueType value) {
    // Если дерево пустое, создаём корень
    if (!map->root) {
        map->root = btree_node_create(true);
    }

    BTreeNode* root = map->root;

    // Проверка на существование ключа
    ValueType existing = btree_search_node(root, key);
    if (existing) {
        // Найдём узел с ключом и обновим
        BTreeNode* node = root;
        while (node) {
            int i = 0;
            while (i < node->n && key > node->keys[i]) i++;
            if (i < node->n && key == node->keys[i]) {
                map_free_value(node->values[i]);
                node->values[i] = copy_value(value);
                return true;
            }
            if (node->leaf) break;
            node = node->children[i];
        }
    }

    // Если корень полон, создаём новый корень
    if (root->n == MAX_KEYS) {
        BTreeNode* new_root = btree_node_create(false);
        new_root->children[0] = root;
        btree_split_child(new_root, 0, root);
        map->root = new_root;
        btree_insert_nonfull(new_root, key, value);
    }
    else {
        btree_insert_nonfull(root, key, value);
    }
    map->size++;
    return true;
}

// Поиск предшественника
static KeyType btree_get_predecessor(BTreeNode* node, int idx) {
    BTreeNode* cur = node->children[idx];
    while (!cur->leaf) cur = cur->children[cur->n];
    return cur->keys[cur->n - 1];
}

// Поиск преемника
static KeyType btree_get_successor(BTreeNode* node, int idx) {
    BTreeNode* cur = node->children[idx + 1];
    while (!cur->leaf) cur = cur->children[0];
    return cur->keys[0];
}

// Заполнение дочернего узла, если в нём меньше MIN_KEYS ключей
static void btree_fill(BTreeNode* node, int idx) {
    // Попытка занять у левого соседа
    if (idx > 0 && node->children[idx - 1]->n > MIN_KEYS) {
        BTreeNode* child = node->children[idx];
        BTreeNode* left_sibling = node->children[idx - 1];

        for (int i = child->n - 1; i >= 0; i--) {
            child->keys[i + 1] = child->keys[i];
            child->values[i + 1] = child->values[i];
        }

        if (!child->leaf) {
            for (int i = child->n; i >= 0; i--) {
                child->children[i + 1] = child->children[i];
            }
        }

        child->keys[0] = node->keys[idx - 1];
        child->values[0] = node->values[idx - 1];

        if (!child->leaf) {
            child->children[0] = left_sibling->children[left_sibling->n];
        }

        node->keys[idx - 1] = left_sibling->keys[left_sibling->n - 1];
        node->values[idx - 1] = left_sibling->values[left_sibling->n - 1];

        child->n++;
        left_sibling->n--;
    }
    // Попытка занять у правого соседа
    else if (idx < node->n && node->children[idx + 1]->n > MIN_KEYS) {
        BTreeNode* child = node->children[idx];
        BTreeNode* right_sibling = node->children[idx + 1];

        child->keys[child->n] = node->keys[idx];
        child->values[child->n] = node->values[idx];

        if (!child->leaf) {
            child->children[child->n + 1] = right_sibling->children[0];
        }

        node->keys[idx] = right_sibling->keys[0];
        node->values[idx] = right_sibling->values[0];

        for (int i = 1; i < right_sibling->n; i++) {
            right_sibling->keys[i - 1] = right_sibling->keys[i];
            right_sibling->values[i - 1] = right_sibling->values[i];
        }

        if (!right_sibling->leaf) {
            for (int i = 1; i <= right_sibling->n; i++) {
                right_sibling->children[i - 1] = right_sibling->children[i];
            }
        }

        child->n++;
        right_sibling->n--;
    }
    // Слияние с соседом
    else {
        if (idx < node->n) {
            BTreeNode* child = node->children[idx];
            BTreeNode* right_sibling = node->children[idx + 1];

            child->keys[child->n] = node->keys[idx];
            child->values[child->n] = node->values[idx];

            for (int i = 0; i < right_sibling->n; i++) {
                child->keys[child->n + 1 + i] = right_sibling->keys[i];
                child->values[child->n + 1 + i] = right_sibling->values[i];
            }

            if (!child->leaf) {
                for (int i = 0; i <= right_sibling->n; i++) {
                    child->children[child->n + 1 + i] = right_sibling->children[i];
                }
            }

            for (int i = idx + 1; i < node->n; i++) {
                node->keys[i - 1] = node->keys[i];
                node->values[i - 1] = node->values[i];
            }

            for (int i = idx + 2; i <= node->n; i++) {
                node->children[i - 1] = node->children[i];
            }

            child->n += right_sibling->n + 1;
            node->n--;
            free(right_sibling);
        }
        else {
            BTreeNode* child = node->children[idx];
            BTreeNode* left_sibling = node->children[idx - 1];

            left_sibling->keys[left_sibling->n] = node->keys[idx - 1];
            left_sibling->values[left_sibling->n] = node->values[idx - 1];

            for (int i = 0; i < child->n; i++) {
                left_sibling->keys[left_sibling->n + 1 + i] = child->keys[i];
                left_sibling->values[left_sibling->n + 1 + i] = child->values[i];
            }

            if (!left_sibling->leaf) {
                for (int i = 0; i <= child->n; i++) {
                    left_sibling->children[left_sibling->n + 1 + i] = child->children[i];
                }
            }

            for (int i = idx; i < node->n; i++) {
                node->keys[i - 1] = node->keys[i];
                node->values[i - 1] = node->values[i];
            }

            for (int i = idx + 1; i <= node->n; i++) {
                node->children[i - 1] = node->children[i];
            }

            left_sibling->n += child->n + 1;
            node->n--;
            free(child);
        }
    }
}

// Удаление ключа из узла
static void btree_remove_from_node(BTreeNode* node, KeyType key) {
    int idx = btree_find_key_index(node, key);

    if (idx < node->n && node->keys[idx] == key) {
        if (node->leaf) {
            map_free_value(node->values[idx]);
            for (int i = idx + 1; i < node->n; i++) {
                node->keys[i - 1] = node->keys[i];
                node->values[i - 1] = node->values[i];
            }
            node->n--;
        }
        else {
            if (node->children[idx]->n > MIN_KEYS) {
                KeyType pred = btree_get_predecessor(node, idx);
                ValueType pred_val = btree_search_node(node->children[idx], pred);
                node->keys[idx] = pred;
                node->values[idx] = copy_value(pred_val);
                btree_remove_from_node(node->children[idx], pred);
            }
            else if (node->children[idx + 1]->n > MIN_KEYS) {
                KeyType succ = btree_get_successor(node, idx);
                ValueType succ_val = btree_search_node(node->children[idx + 1], succ);
                node->keys[idx] = succ;
                node->values[idx] = copy_value(succ_val);
                btree_remove_from_node(node->children[idx + 1], succ);
            }
            else {
                btree_fill(node, idx);
                btree_remove_from_node(node->children[idx], key);
            }
        }
    }
    else {
        if (node->leaf) return;

        bool is_last = (idx == node->n);
        if (node->children[idx]->n == MIN_KEYS) {
            btree_fill(node, idx);
        }
        if (is_last && idx > node->n) {
            btree_remove_from_node(node->children[idx - 1], key);
        }
        else {
            btree_remove_from_node(node->children[idx], key);
        }
    }
}

// Удаление из B-дерева
static bool btree_erase(BTreeMap* map, KeyType key) {
    if (!map->root) return false;

    btree_remove_from_node(map->root, key);

    if (map->root->n == 0) {
        BTreeNode* old_root = map->root;
        if (map->root->leaf) {
            map->root = NULL;
        }
        else {
            map->root = map->root->children[0];
        }
        free(old_root);
    }

    map->size--;
    return true;
}

// Обход дерева
static void btree_traverse_node(BTreeNode* node, void (*visit)(KeyType, ValueType)) {
    if (!node) return;
    int i;
    for (i = 0; i < node->n; i++) {
        if (!node->leaf) btree_traverse_node(node->children[i], visit);
        visit(node->keys[i], node->values[i]);
    }
    if (!node->leaf) btree_traverse_node(node->children[i], visit);
}

// Высота дерева
static size_t btree_height_node(BTreeNode* node) {
    if (!node || node->n == 0) return 0;
    if (node->leaf) return 1;
    return 1 + btree_height_node(node->children[0]);
}

// ----- MapOps -----
static void* btree_create(void) {
    BTreeMap* map = malloc(sizeof(BTreeMap));
    map->root = NULL;
    map->size = 0;
    return map;
}

static void btree_destroy(void* map_ptr) {
    BTreeMap* map = (BTreeMap*)map_ptr;
    btree_node_destroy(map->root);
    free(map);
}

static bool btree_insert_ops(void* map_ptr, KeyType key, ValueType value) {
    BTreeMap* map = (BTreeMap*)map_ptr;
    return btree_insert(map, key, value);
}

static ValueType btree_find(void* map_ptr, KeyType key) {
    BTreeMap* map = (BTreeMap*)map_ptr;
    return btree_search_node(map->root, key);
}

static bool btree_erase_ops(void* map_ptr, KeyType key) {
    BTreeMap* map = (BTreeMap*)map_ptr;
    return btree_erase(map, key);
}

static void btree_traverse(void* map_ptr, void (*visit)(KeyType, ValueType)) {
    BTreeMap* map = (BTreeMap*)map_ptr;
    btree_traverse_node(map->root, visit);
}

static size_t btree_height(void* map_ptr) {
    BTreeMap* map = (BTreeMap*)map_ptr;
    return btree_height_node(map->root);
}

static const MapOps btree_ops = {
    .create = btree_create,
    .destroy = btree_destroy,
    .insert = btree_insert_ops,
    .find = btree_find,
    .erase = btree_erase_ops,
    .traverse = btree_traverse,
    .height = btree_height
};

// ----- TreeImpl -----
static bool btree_find_with_path(void* map_ptr, KeyType key, KeyType* path_keys, int* path_len, int max_path) {
    BTreeMap* map = (BTreeMap*)map_ptr;
    BTreeNode* node = map->root;
    *path_len = 0;

    while (node) {
        int i = 0;
        while (i < node->n && key > node->keys[i]) {
            if (*path_len < max_path) {
                path_keys[(*path_len)++] = node->keys[i];
            }
            i++;
        }

        if (i < node->n && key == node->keys[i]) {
            if (*path_len < max_path) {
                path_keys[(*path_len)++] = node->keys[i];
            }
            return true;
        }

        if (i < node->n && *path_len < max_path) {
            path_keys[(*path_len)++] = node->keys[i];
        }

        if (node->leaf) break;
        node = node->children[i];
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

// Каскадная отрисовка с чередованием направления
static void btree_draw_node_cascade(BTreeNode* node, int x, int y, int dx, bool cascade_right) {
    if (!node || node->n == 0) return;

    int cell_width = 45;
    int cell_height = 35;
    int node_width = node->n * cell_width;
    int start_x = x - node_width / 2;
    int vertical_spacing = 85;

    // Отрисовка прямоугольника узла
    DrawRectangleLines(start_x, y - cell_height / 2, node_width, cell_height, DARKBLUE);

    // Отрисовка разделительных линий
    for (int i = 1; i < node->n; i++) {
        DrawLine(start_x + i * cell_width, y - cell_height / 2,
            start_x + i * cell_width, y + cell_height / 2, DARKBLUE);
    }

    // Отрисовка ключей
    for (int i = 0; i < node->n; i++) {
        bool is_found = false;
        Color bg_color = LIGHTGRAY;
        Color text_color = BLACK;

        if (is_key_in_path(node->keys[i], &is_found)) {
            if (is_found) {
                bg_color = LIME;
            }
            else {
                bg_color = (Color){ 255, 136, 136, 255 };
            }
        }

        DrawRectangle(start_x + i * cell_width + 1, y - cell_height / 2 + 1,
            cell_width - 2, cell_height - 2, bg_color);

        char key_str[16];
        sprintf(key_str, "%d", node->keys[i]);
        int text_width = MeasureText(key_str, 16);
        DrawText(key_str, start_x + i * cell_width + (cell_width - text_width) / 2,
            y - 8, 16, text_color);
    }

    // Отрисовка дочерних узлов с каскадом
    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++) {
            if (node->children[i] && node->children[i]->n > 0) {
                int child_y = y + vertical_spacing;
                int child_x;

                if (cascade_right) {
                    child_x = x + i * dx;
                    child_y += i * 35;
                }
                else {
                    child_x = x - i * dx;
                    child_y += i * 35;
                }

                // Линия от родителя к потомку
                DrawLine(start_x + i * cell_width, y + cell_height / 2,
                    child_x, child_y - cell_height / 2, DARKGRAY);

                // Рекурсивный вызов с ПРОТИВОПОЛОЖНЫМ направлением каскада
                btree_draw_node_cascade(node->children[i], child_x, child_y, (int)(dx / 1.4f), !cascade_right);
            }
        }
    }
}

static void btree_draw(void* map_ptr, int x, int y, int dx) {
    BTreeMap* map = (BTreeMap*)map_ptr;
    if (map->root && map->root->n > 0) {
        btree_draw_node_cascade(map->root, x, y, dx, true);
    }
}
#else
static void btree_draw(void* map_ptr, int x, int y, int dx) {
    (void)map_ptr; (void)x; (void)y; (void)dx;
}
#endif

const TreeImpl btree_tree_impl = {
    .ops = &btree_ops,
    .draw = btree_draw,
    .find_with_path = btree_find_with_path
};

const TreeImpl* get_btree_tree_impl(void) {
    return &btree_tree_impl;
}
