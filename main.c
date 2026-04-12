#define _CRT_SECURE_NO_WARNINGS
#include "raylib.h"
#include "map.h"
#include "bst_map.h"
#include "btree_map.h"
#include "rb_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

// Тип уведомления
typedef enum {
    NOTIFY_NONE,
    NOTIFY_INSERT_SUCCESS,
    NOTIFY_INSERT_REPLACE,
    NOTIFY_DELETE_SUCCESS,
    NOTIFY_DELETE_NOT_FOUND,
    NOTIFY_SEARCH_FOUND,
    NOTIFY_SEARCH_NOT_FOUND
} NotificationType;

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

// Структура для хранения состояния одного дерева
typedef struct {
    void* map;
    int tree_offset_x;
    int tree_offset_y;
    KeyType search_path[MAX_PATH];
    int search_path_len;
    bool search_result;
    ValueType found_value;
} TreeState;

typedef struct {
    const TreeImpl* impls[3];        // реализации деревьев
    TreeState states[3];             // состояния каждого дерева
    int current_impl;                // текущий индекс
    char input[64];
    bool show_input;
    InputState input_state;
    int insert_key;
    ProgramMode mode;
    int traverse_scroll;

    // Система уведомлений
    NotificationType notification;
    double notification_time;
    int notification_key;
    char notification_value[64];
} AppState;

static AppState state;

static char* safe_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* p = malloc(len);
    if (p) memcpy(p, s, len);
    return p;
}

// Получить текущее дерево
void* get_current_map(void) {
    return state.states[state.current_impl].map;
}

// Получить текущую реализацию
const TreeImpl* get_current_impl(void) {
    return state.impls[state.current_impl];
}

// Показать уведомление
void show_notification(NotificationType type, int key, const char* value) {
    state.notification = type;
    state.notification_time = GetTime();
    state.notification_key = key;
    if (value) {
        strncpy(state.notification_value, value, 63);
        state.notification_value[63] = '\0';
    }
    else {
        state.notification_value[0] = '\0';
    }
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

// Сброс состояния поиска для текущего дерева
void reset_search_state(void) {
    TreeState* ts = &state.states[state.current_impl];
    ts->search_path_len = 0;
    ts->search_result = false;
    ts->found_value = NULL;
    g_search_path_len = 0;
    g_search_found = false;
}

// Синхронизация глобальных переменных поиска с текущим деревом
void sync_search_state(void) {
    TreeState* ts = &state.states[state.current_impl];
    memcpy(g_search_path, ts->search_path, ts->search_path_len * sizeof(KeyType));
    g_search_path_len = ts->search_path_len;
    g_search_found = ts->search_result;
}

// Отрисовка уведомления
void draw_notification(void) {
    if (state.notification == NOTIFY_NONE) return;

    double current_time = GetTime();
    if (current_time - state.notification_time > 3.0) {
        state.notification = NOTIFY_NONE;
        return;
    }

    const char* message = NULL;
    Color msg_color = BLACK;

    switch (state.notification) {
    case NOTIFY_INSERT_SUCCESS:
        message = TextFormat("Inserted: %d -> %s", state.notification_key, state.notification_value);
        msg_color = DARKGREEN;
        break;
    case NOTIFY_INSERT_REPLACE:
        message = TextFormat("Replaced: %d -> %s", state.notification_key, state.notification_value);
        msg_color = DARKBLUE;
        break;
    case NOTIFY_DELETE_SUCCESS:
        message = TextFormat("Deleted: %d", state.notification_key);
        msg_color = DARKGREEN;
        break;
    case NOTIFY_DELETE_NOT_FOUND:
        message = TextFormat("Key %d not found for deletion", state.notification_key);
        msg_color = RED;
        break;
    case NOTIFY_SEARCH_FOUND:
        message = TextFormat("Found: %d -> %s", state.notification_key, state.notification_value);
        msg_color = DARKGREEN;
        break;
    case NOTIFY_SEARCH_NOT_FOUND:
        message = TextFormat("Key %d not found", state.notification_key);
        msg_color = RED;
        break;
    default:
        break;
    }

    if (message) {
        int text_width = MeasureText(message, 20);
        int box_x = SCREEN_WIDTH / 2 - text_width / 2 - 20;
        int box_y = SCREEN_HEIGHT - 100;

        DrawRectangle(box_x, box_y, text_width + 40, 40, (Color) { 240, 240, 240, 230 });
        DrawRectangleLines(box_x, box_y, text_width + 40, 40, msg_color);
        DrawText(message, box_x + 20, box_y + 10, 20, msg_color);
    }
}

// Отрисовка окна обхода
void draw_traverse_window(void) {
    DrawText("Tree Traversal (key: value)", 10, 10, 24, DARKBLUE);
    DrawText("Use mouse wheel or arrows to scroll", 10, 45, 18, DARKGRAY);
    DrawText("Press ENTER to return", 10, 70, 18, DARKGRAY);

    DrawLine(10, 95, SCREEN_WIDTH - 10, 95, LIGHTGRAY);

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

    DrawText(TextFormat("Total entries: %d", g_traverse_count), 10, SCREEN_HEIGHT - 30, 18, DARKGREEN);

    if (g_traverse_count > max_visible) {
        float scroll_percent = (float)state.traverse_scroll / (g_traverse_count - max_visible);
        int scroll_bar_height = SCREEN_HEIGHT - 140;
        int scroll_handle_y = 100 + (int)(scroll_percent * (float)scroll_bar_height);

        DrawRectangle(SCREEN_WIDTH - 20, 100, 10, scroll_bar_height, LIGHTGRAY);
        DrawRectangle(SCREEN_WIDTH - 20, scroll_handle_y, 10, 50, GRAY);
    }
}

void draw_search_path_info(void) {
    TreeState* ts = &state.states[state.current_impl];
    char path_str[256] = "Path: ";
    for (int i = 0; i < ts->search_path_len; i++) {
        char tmp[16];
        sprintf(tmp, "%d ", ts->search_path[i]);
        strcat(path_str, tmp);
    }
    DrawText(path_str, 10, SCREEN_HEIGHT - 40, 20, ts->search_result ? GREEN : RED);
    if (ts->search_result && ts->found_value) {
        DrawText(TextFormat("Found: %d -> %s", ts->search_path[ts->search_path_len - 1], ts->found_value),
            10, SCREEN_HEIGHT - 70, 20, DARKGREEN);
    }
    else if (!ts->search_result) {
        DrawText("Not found", 10, SCREEN_HEIGHT - 70, 20, RED);
    }
}

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Map Visualizer (BST, B-Tree, RB-Tree)");
    SetTargetFPS(60);
    SetExitKey(0);

    // Инициализация реализаций
    state.impls[0] = get_bst_tree_impl();
    state.impls[1] = get_btree_tree_impl();
    state.impls[2] = get_rb_tree_impl();
    const char* impl_names[] = { "BST", "B-Tree", "RB-Tree" };
    const int impl_count = 3;

    // Инициализация состояний всех деревьев
    for (int i = 0; i < impl_count; i++) {
        state.states[i].map = state.impls[i]->ops->create();
        state.states[i].tree_offset_x = 0;
        state.states[i].tree_offset_y = 0;
        state.states[i].search_path_len = 0;
        state.states[i].search_result = false;
        state.states[i].found_value = NULL;
    }

    state.current_impl = 0;
    state.show_input = false;
    state.input_state = INPUT_IDLE;
    state.mode = MODE_MAIN;
    state.traverse_scroll = 0;
    state.input[0] = '\0';
    state.notification = NOTIFY_NONE;

    Rectangle input_box = { 10, 240, 200, 30 };

    while (!WindowShouldClose()) {
        TreeState* ts = &state.states[state.current_impl];
        const TreeImpl* impl = state.impls[state.current_impl];

        // ========== ОБРАБОТКА ПЕРЕМЕЩЕНИЯ ДЕРЕВА ==========
        if (state.mode == MODE_MAIN && !state.show_input) {
            int move_speed = 6;
            if (IsKeyDown(KEY_LEFT)) ts->tree_offset_x += move_speed;
            if (IsKeyDown(KEY_RIGHT)) ts->tree_offset_x -= move_speed;
            if (IsKeyDown(KEY_UP)) ts->tree_offset_y += move_speed;
            if (IsKeyDown(KEY_DOWN)) ts->tree_offset_y -= move_speed;

            if (IsKeyPressed(KEY_R)) {
                ts->tree_offset_x = 0;
                ts->tree_offset_y = 0;
            }
        }

        // ========== ОБРАБОТКА ESC ДЛЯ ЗАКРЫТИЯ ПРОГРАММЫ ==========
        if (state.mode == MODE_MAIN && state.input_state == INPUT_IDLE) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                break;
            }
        }

        // ========== ОБРАБОТКА ВВОДА ==========
        if (state.mode == MODE_MAIN) {
            if (state.input_state == INPUT_IDLE) {
                int old_impl = state.current_impl;
                if (IsKeyPressed(KEY_ONE)) state.current_impl = 0;
                if (IsKeyPressed(KEY_TWO)) state.current_impl = 1;
                if (IsKeyPressed(KEY_THREE)) state.current_impl = 2;
                if (IsKeyPressed(KEY_TAB)) state.current_impl = (state.current_impl + 1) % impl_count;

                if (old_impl != state.current_impl) {
                    // При переключении синхронизируем глобальный путь поиска
                    sync_search_state();
                    g_traverse_count = 0;
                }

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
                    impl->ops->traverse(ts->map, traverse_callback);
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

                if (IsKeyPressed(KEY_BACKSPACE) && strlen(state.input) > 0) {
                    state.input[strlen(state.input) - 1] = '\0';
                }

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

                        ValueType old_value = impl->ops->find(ts->map, state.insert_key);
                        bool existed = (old_value != NULL);

                        impl->ops->insert(ts->map, state.insert_key, safe_strdup(value));

                        if (existed) {
                            show_notification(NOTIFY_INSERT_REPLACE, state.insert_key, value);
                        }
                        else {
                            show_notification(NOTIFY_INSERT_SUCCESS, state.insert_key, value);
                        }

                        state.input_state = INPUT_IDLE;
                        state.show_input = false;
                        state.input[0] = '\0';
                    }
                    break;

                    case INPUT_DELETE:
                    {
                        int num = atoi(state.input);
                        ValueType old_value = impl->ops->find(ts->map, num);

                        if (old_value) {
                            impl->ops->erase(ts->map, num);
                            show_notification(NOTIFY_DELETE_SUCCESS, num, NULL);
                        }
                        else {
                            show_notification(NOTIFY_DELETE_NOT_FOUND, num, NULL);
                        }

                        state.input_state = INPUT_IDLE;
                        state.show_input = false;
                        state.input[0] = '\0';
                    }
                    break;

                    case INPUT_SEARCH:
                    {
                        int num = atoi(state.input);
                        ts->search_result = impl->find_with_path(ts->map, num,
                            ts->search_path, &ts->search_path_len, MAX_PATH);
                        ts->found_value = impl->ops->find(ts->map, num);

                        sync_search_state();

                        if (ts->search_result) {
                            show_notification(NOTIFY_SEARCH_FOUND, num, ts->found_value);
                        }
                        else {
                            show_notification(NOTIFY_SEARCH_NOT_FOUND, num, NULL);
                        }

                        state.input_state = INPUT_IDLE;
                        state.show_input = false;
                        state.input[0] = '\0';
                    }
                    break;

                    default:
                        break;
                    }
                }

                if (IsKeyPressed(KEY_ESCAPE)) {
                    state.input_state = INPUT_IDLE;
                    state.show_input = false;
                    state.input[0] = '\0';
                }
            }
        }
        else if (state.mode == MODE_TRAVERSE) {
            if (IsKeyPressed(KEY_ENTER)) {
                state.mode = MODE_MAIN;
            }

            int wheel = GetMouseWheelMove();
            if (wheel != 0) {
                state.traverse_scroll -= wheel;
                int max_visible = (SCREEN_HEIGHT - 130) / 25;
                int max_scroll = g_traverse_count - max_visible;
                if (max_scroll < 0) max_scroll = 0;
                if (state.traverse_scroll < 0) state.traverse_scroll = 0;
                if (state.traverse_scroll > max_scroll) state.traverse_scroll = max_scroll;
            }

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
            DrawText(TextFormat("Current: %s", impl_names[state.current_impl]), 10, 80, 20, DARKGREEN);
            DrawText("W - insert", 10, 115, 20, DARKGRAY);
            DrawText("D - delete", 10, 145, 20, DARKGRAY);
            DrawText("S - search", 10, 175, 20, DARKGRAY);
            DrawText("T - traverse", 10, 205, 20, DARKGRAY);
            DrawText("Arrows - move tree", 10, 310, 20, DARKGRAY);
            DrawText("R - reset position", 10, 330, 20, DARKGRAY);
            DrawText("ESC - exit (when not typing)", 10, 350, 20, DARKGRAY);

            if (state.input_state == INPUT_INSERT_KEY)
                DrawText("INSERT: Enter key", 250, 115, 20, GREEN);
            else if (state.input_state == INPUT_INSERT_VALUE)
                DrawText(TextFormat("INSERT: Enter value for key %d", state.insert_key), 250, 115, 20, GREEN);
            else if (state.input_state == INPUT_DELETE)
                DrawText("DELETE: Enter key", 250, 145, 20, RED);
            else if (state.input_state == INPUT_SEARCH)
                DrawText("SEARCH: Enter key", 250, 175, 20, BLUE);

            if (state.show_input) {
                DrawRectangleRec(input_box, LIGHTGRAY);
                DrawRectangleLines((int)input_box.x, (int)input_box.y,
                    (int)input_box.width, (int)input_box.height, DARKBLUE);
                DrawText(state.input, (int)input_box.x + 5, (int)input_box.y + 5, 20, BLACK);

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

            size_t h = impl->ops->height(ts->map);
            DrawText(TextFormat("Height: %zu", h), SCREEN_WIDTH - 150, 10, 20, DARKGRAY);

            impl->draw(ts->map, SCREEN_WIDTH / 2 + ts->tree_offset_x,
                150 + ts->tree_offset_y, 300);

            if (ts->search_path_len > 0) {
                draw_search_path_info();
            }

            draw_notification();
        }
        else if (state.mode == MODE_TRAVERSE) {
            draw_traverse_window();
        }

        EndDrawing();
    }

    // Очистка всех деревьев
    for (int i = 0; i < impl_count; i++) {
        state.impls[i]->ops->destroy(state.states[i].map);
    }

    CloseWindow();
    return 0;
}