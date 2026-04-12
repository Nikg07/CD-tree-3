#define _CRT_SECURE_NO_WARNINGS
#include "raylib.h"
#include "map.h"
#include "bst_map.h"
// #include "btree_map.h"
#include "rb_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH 1512
#define SCREEN_HEIGHT 982
#define MAX_PATH 100
#define MAX_TRAVERSE_LINES 1000

// Глобальные переменные для подсветки пути
KeyType g_search_path[MAX_PATH];
int g_search_path_len;
bool g_search_found;

// Переменные для вывода обхода
typedef struct {
    KeyType key;
    char value[64];
} TraverseEntry;

TraverseEntry g_traverse_entries[MAX_TRAVERSE_LINES];
int g_traverse_count = 0;

// Состояния ввода
typedef enum {
    INPUT_IDLE,
    INPUT_INSERT_KEY,
    INPUT_INSERT_VALUE,
    INPUT_DELETE,
    INPUT_SEARCH
} InputState;

// Режимы программы
typedef enum {
    MODE_MAIN,
    MODE_TRAVERSE
} ProgramMode;

typedef struct {
    const TreeImpl* impl;
    void* map;
    char input[64];
    bool show_input;
    InputState input_state;
    int insert_key;
    KeyType search_path[MAX_PATH];
    int search_path_len;
    bool search_result;
    ValueType found_value;
    ProgramMode mode;
    int traverse_scroll;
    int tree_offset_x;  
    int tree_offset_y;  
} AppState;

static AppState state;

static char* safe_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* p = malloc(len);
    if (p) memcpy(p, s, len);
    return p;
}

// Функция обратного вызова для обхода
void traverse_callback(KeyType key, ValueType value) {
    if (g_traverse_count < MAX_TRAVERSE_LINES) {
        g_traverse_entries[g_traverse_count].key = key;
        strncpy(g_traverse_entries[g_traverse_count].value, value, 63);
        g_traverse_entries[g_traverse_count].value[63] = '\0';
        g_traverse_count++;
    }
}

// Сброс состояния поиска
void reset_search_state(void) {
    state.search_path_len = 0;
    state.search_result = false;
    state.found_value = NULL;
    g_search_path_len = 0;
    g_search_found = false;
}

// Отрисовка окна обхода
void draw_traverse_window(void) {
    // Заголовок
    DrawText("Tree Traversal (key: value)", 10, 10, 24, DARKBLUE);
    DrawText("Use mouse wheel or arrows to scroll", 10, 45, 18, DARKGRAY);
    DrawText("Press ENTER to return", 10, 70, 18, DARKGRAY);

    // Разделительная линия
    DrawLine(10, 95, SCREEN_WIDTH - 10, 95, LIGHTGRAY);

    // Отрисовка записей с учётом прокрутки
    int start_index = state.traverse_scroll;
    int max_visible = (SCREEN_HEIGHT - 130) / 25;

    for (int i = 0; i < max_visible && (start_index + i) < g_traverse_count; i++) {
        int idx = start_index + i;
        char line[128];
        snprintf(line, sizeof(line), "%d: %s", g_traverse_entries[idx].key, g_traverse_entries[idx].value);

        if (idx % 2 == 0) {
            DrawRectangle(10, 100 + i * 25, SCREEN_WIDTH - 20, 25, (Color) { 240, 240, 240, 255 });
        }
        DrawText(line, 20, 105 + i * 25, 20, DARKGRAY);
    }

    // Информация о количестве записей
    DrawText(TextFormat("Total entries: %d", g_traverse_count), 10, SCREEN_HEIGHT - 30, 18, DARKGREEN);

    // Полоса прокрутки
    if (g_traverse_count > max_visible) {
        float scroll_percent = (float)state.traverse_scroll / (g_traverse_count - max_visible);
        int scroll_bar_height = SCREEN_HEIGHT - 140;
        int scroll_handle_y = 100 + (int)(scroll_percent * (float)scroll_bar_height);  

        DrawRectangle(SCREEN_WIDTH - 20, 100, 10, scroll_bar_height, LIGHTGRAY);
        DrawRectangle(SCREEN_WIDTH - 20, scroll_handle_y, 10, 50, GRAY);
    }
}

void draw_search_path_info(void) {
    char path_str[256] = "Path: ";
    for (int i = 0; i < state.search_path_len; i++) {
        char tmp[16];
        sprintf(tmp, "%d ", state.search_path[i]);
        strcat(path_str, tmp);
    }
    DrawText(path_str, 10, SCREEN_HEIGHT - 40, 20, state.search_result ? GREEN : RED);
    if (state.search_result && state.found_value) {
        DrawText(TextFormat("Found: %d -> %s", state.search_path[state.search_path_len - 1], state.found_value),
            10, SCREEN_HEIGHT - 70, 20, DARKGREEN);
    }
    else if (!state.search_result) {
        DrawText("Not found", 10, SCREEN_HEIGHT - 70, 20, RED);
    }
}

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Map Visualizer (BST, B-Tree, RB-Tree)");
    SetTargetFPS(60);
    SetExitKey(0);

    const TreeImpl* implementations[] = {
        get_bst_tree_impl(),
        // get_btree_tree_impl(),
        get_rb_tree_impl()
    };
    const char* impl_names[] = { "BST", "B-Tree", "RB-Tree" };
    const int impl_count = 2;

    int current_impl = 0;
    state.impl = implementations[current_impl];
    state.map = state.impl->ops->create();
    state.show_input = false;
    state.input_state = INPUT_IDLE;
    state.search_path_len = 0;
    state.search_result = false;
    state.found_value = NULL;
    state.mode = MODE_MAIN;
    state.traverse_scroll = 0;
    state.input[0] = '\0';
    state.tree_offset_x = 0;  
    state.tree_offset_y = 0;  

    Rectangle input_box = { 10, 240, 200, 30 };

    while (!WindowShouldClose()) {
        // ========== ОБРАБОТКА ПЕРЕМЕЩЕНИЯ ДЕРЕВА (всегда, когда не ввод текста) ==========
        if (state.mode == MODE_MAIN && !state.show_input) {
            int move_speed = 6;
            if (IsKeyDown(KEY_LEFT)) state.tree_offset_x += move_speed;
            if (IsKeyDown(KEY_RIGHT)) state.tree_offset_x -= move_speed;
            if (IsKeyDown(KEY_UP)) state.tree_offset_y += move_speed;      
            if (IsKeyDown(KEY_DOWN)) state.tree_offset_y -= move_speed;    

            // Сброс позиции по R
            if (IsKeyPressed(KEY_R)) {
                state.tree_offset_x = 0;
                state.tree_offset_y = 0;
            }
        }

        // ========== ОБРАБОТКА ESC ДЛЯ ЗАКРЫТИЯ ПРОГРАММЫ ==========
        // ESC закрывает программу только в главном окне и когда нет активного ввода
        if (state.mode == MODE_MAIN && state.input_state == INPUT_IDLE) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                break;  // выход из главного цикла
            }
        }

        // ========== ОБРАБОТКА ВВОДА В ЗАВИСИМОСТИ ОТ РЕЖИМА ==========
        if (state.mode == MODE_MAIN) {
            // Команды (только когда нет активного ввода)
            if (state.input_state == INPUT_IDLE) {
                int old_impl = current_impl;
                if (IsKeyPressed(KEY_ONE)) current_impl = 0;
                if (IsKeyPressed(KEY_TWO) && impl_count > 1) current_impl = 1;
                if (IsKeyPressed(KEY_THREE) && impl_count > 2) current_impl = 2;
                if (IsKeyPressed(KEY_TAB)) current_impl = (current_impl + 1) % impl_count;
                if (old_impl != current_impl) {
                    state.impl->ops->destroy(state.map);
                    state.impl = implementations[current_impl];
                    state.map = state.impl->ops->create();
                    reset_search_state();
                    g_traverse_count = 0;
                }

                // Команды операций
                if (IsKeyPressed(KEY_W)) {
                    state.input_state = INPUT_INSERT_KEY;
                    state.show_input = true;
                    state.input[0] = '\0';
                    reset_search_state();
                }
                if (IsKeyPressed(KEY_D)) {
                    state.input_state = INPUT_DELETE;
                    state.show_input = true;
                    state.input[0] = '\0';
                    reset_search_state();
                }
                if (IsKeyPressed(KEY_S)) {
                    state.input_state = INPUT_SEARCH;
                    state.show_input = true;
                    state.input[0] = '\0';
                }
                if (IsKeyPressed(KEY_T)) {
                    g_traverse_count = 0;
                    state.impl->ops->traverse(state.map, traverse_callback);
                    state.mode = MODE_TRAVERSE;
                    state.traverse_scroll = 0;
                    reset_search_state();
                }
            }

            // Ввод текста
            if (state.show_input) {
                int ch = GetCharPressed();
                while (ch > 0) {
                    if (state.input_state == INPUT_INSERT_KEY ||
                        state.input_state == INPUT_DELETE ||
                        state.input_state == INPUT_SEARCH) {
                        if (ch >= '0' && ch <= '9' && strlen(state.input) < 15) {
                            char s[2] = { (char)ch, '\0' };
                            strcat(state.input, s);
                        }
                    }
                    else if (state.input_state == INPUT_INSERT_VALUE) {
                        if (strlen(state.input) < 63) {
                            char s[2] = { (char)ch, '\0' };
                            strcat(state.input, s);
                        }
                    }
                    ch = GetCharPressed();
                }

                // Backspace
                if (IsKeyPressed(KEY_BACKSPACE) && strlen(state.input) > 0) {
                    state.input[strlen(state.input) - 1] = '\0';
                }

                // Enter - завершение ввода
                if (IsKeyPressed(KEY_ENTER) && strlen(state.input) > 0) {
                    switch (state.input_state) {
                    case INPUT_INSERT_KEY:
                        state.insert_key = atoi(state.input);
                        state.input_state = INPUT_INSERT_VALUE;
                        state.input[0] = '\0';
                        break;

                    case INPUT_INSERT_VALUE:
                    {
                        char* value = state.input;
                        if (value[0] == '\0') {
                            sprintf(state.input, "value_%d", state.insert_key);
                            value = state.input;
                        }
                        state.impl->ops->insert(state.map, state.insert_key, safe_strdup(value));
                        state.input_state = INPUT_IDLE;
                        state.show_input = false;
                        state.input[0] = '\0';
                    }
                    break;

                    case INPUT_DELETE:
                    {
                        int num = atoi(state.input);
                        state.impl->ops->erase(state.map, num);
                        state.input_state = INPUT_IDLE;
                        state.show_input = false;
                        state.input[0] = '\0';
                    }
                    break;

                    case INPUT_SEARCH:
                    {
                        int num = atoi(state.input);
                        state.search_result = state.impl->find_with_path(state.map, num,
                            state.search_path, &state.search_path_len, MAX_PATH);
                        state.found_value = state.impl->ops->find(state.map, num);
                        memcpy(g_search_path, state.search_path, state.search_path_len * sizeof(KeyType));
                        g_search_path_len = state.search_path_len;
                        g_search_found = state.search_result;
                        state.input_state = INPUT_IDLE;
                        state.show_input = false;
                        state.input[0] = '\0';
                    }
                    break;

                    default:
                        break;
                    }
                }

                // ESC - отмена ввода (НЕ закрывает программу)
                if (IsKeyPressed(KEY_ESCAPE)) {
                    state.input_state = INPUT_IDLE;
                    state.show_input = false;
                    state.input[0] = '\0';
                }
            }
        }
        else if (state.mode == MODE_TRAVERSE) {
            // Управление в режиме обхода
            if (IsKeyPressed(KEY_ENTER)) {
                state.mode = MODE_MAIN;
            }
            // ESC в режиме обхода игнорируется (ничего не делает)

            // Прокрутка колесиком мыши
            int wheel = GetMouseWheelMove();
            if (wheel != 0) {
                state.traverse_scroll -= wheel;
                int max_visible = (SCREEN_HEIGHT - 130) / 25;
                int max_scroll = g_traverse_count - max_visible;
                if (max_scroll < 0) max_scroll = 0;
                if (state.traverse_scroll < 0) state.traverse_scroll = 0;
                if (state.traverse_scroll > max_scroll) state.traverse_scroll = max_scroll;
            }

            // Прокрутка стрелками
            if (IsKeyDown(KEY_UP)) {
                state.traverse_scroll--;
                if (state.traverse_scroll < 0) state.traverse_scroll = 0;
            }
            if (IsKeyDown(KEY_DOWN)) {
                int max_visible = (SCREEN_HEIGHT - 130) / 25;
                int max_scroll = g_traverse_count - max_visible;
                if (max_scroll < 0) max_scroll = 0;
                state.traverse_scroll++;
                if (state.traverse_scroll > max_scroll) state.traverse_scroll = max_scroll;
            }
        }

        // ========== ОТРИСОВКА ==========
        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (state.mode == MODE_MAIN) {
            DrawText("Map Visualizer", 10, 10, 24, DARKBLUE);
            DrawText("1-BST  2-B-Tree  3-RB-Tree  (TAB to switch)", 10, 45, 20, DARKGRAY);
            DrawText(TextFormat("Current: %s", impl_names[current_impl]), 10, 80, 20, DARKGREEN);
            DrawText("W - insert", 10, 115, 20, DARKGRAY);
            DrawText("D - delete", 10, 145, 20, DARKGRAY);
            DrawText("S - search", 10, 175, 20, DARKGRAY);
            DrawText("T - traverse", 10, 205, 20, DARKGRAY);
            DrawText("Arrows - move tree", 10, 310, 20, DARKGRAY);        
            DrawText("R - reset position", 10, 330, 20, DARKGRAY);       
            DrawText("ESC - exit (when not typing)", 10, 350, 20, DARKGRAY);

            // Статус операции
            if (state.input_state == INPUT_INSERT_KEY)
                DrawText("INSERT: Enter key", 250, 115, 20, GREEN);
            else if (state.input_state == INPUT_INSERT_VALUE)
                DrawText(TextFormat("INSERT: Enter value for key %d", state.insert_key), 250, 115, 20, GREEN);
            else if (state.input_state == INPUT_DELETE)
                DrawText("DELETE: Enter key", 250, 145, 20, RED);
            else if (state.input_state == INPUT_SEARCH)
                DrawText("SEARCH: Enter key", 250, 175, 20, BLUE);

            // Поле ввода
            if (state.show_input) {
                DrawRectangleRec(input_box, LIGHTGRAY);
                DrawRectangleLines((int)input_box.x, (int)input_box.y,
                    (int)input_box.width, (int)input_box.height, DARKBLUE);
                DrawText(state.input, (int)input_box.x + 5, (int)input_box.y + 5, 20, BLACK);

                // Подсказка
                if (state.input_state == INPUT_INSERT_KEY ||
                    state.input_state == INPUT_DELETE ||
                    state.input_state == INPUT_SEARCH) {
                    DrawText("Enter key (numbers only)", (int)input_box.x, (int)input_box.y + 35, 16, GRAY);
                }
                else if (state.input_state == INPUT_INSERT_VALUE) {
                    DrawText("Enter value (any characters)", (int)input_box.x, (int)input_box.y + 35, 16, GRAY);
                }
            }
            else {
                DrawText("Press W/D/S/T to operate", (int)input_box.x, (int)input_box.y - 10, 20, GRAY);
            }

            size_t h = state.impl->ops->height(state.map);
            DrawText(TextFormat("Height: %zu", h), SCREEN_WIDTH - 150, 10, 20, DARKGRAY);

            // Отрисовка дерева с учётом смещения  ← ИЗМЕНЕНО
            state.impl->draw(state.map, SCREEN_WIDTH / 2 + state.tree_offset_x,
                150 + state.tree_offset_y, 300);

            // Вывод результатов поиска
            if (state.search_path_len > 0) {
                draw_search_path_info();
            }
        }
        else if (state.mode == MODE_TRAVERSE) {
            draw_traverse_window();
        }

        EndDrawing();
    }

    state.impl->ops->destroy(state.map);
    CloseWindow();
    return 0;
}