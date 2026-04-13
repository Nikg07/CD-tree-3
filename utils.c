#define _CRT_SECURE_NO_WARNINGS
#include "map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef VISUALIZATION
#include "raylib.h"
#endif

void map_free_value(ValueType val) {
    free(val);
}

char* safe_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* p = malloc(len);
    if (p) memcpy(p, s, len);
    return p;
}

// Структура для хранения пары ключ-значение
typedef struct {
    KeyType key;
    char value[64];
} KeyValuePair;

// Функция сравнения для qsort
int compare_pairs(const void* a, const void* b) {
    KeyValuePair* pa = (KeyValuePair*)a;
    KeyValuePair* pb = (KeyValuePair*)b;
    return pa->key - pb->key;
}

// Структура для метрик
typedef struct {
    double insert_time;
    double search_time;
    double erase_time;
    size_t height_before;
    size_t height_after;
} TestMetrics;

void test_map_implementation(const MapOps* ops, const char* name,
    KeyValuePair* pairs, int n,
    TestMetrics* metrics, FILE* out) {
    printf("Testing %s...\n", name);
    fprintf(out, "%s:\n", name);

    // Определяем количество повторений для точности
    // Чем меньше данных, тем больше повторений
    int repeats;
    if (n < 100) repeats = 1000;
    else if (n < 1000) repeats = 100;
    else if (n < 5000) repeats = 20;
    else repeats = 5;

    double start, end;

    // ========== ВРЕМЯ ВСТАВКИ ==========
    start = GetTime();
    for (int r = 0; r < repeats; r++) {
        void* test_map = ops->create();
        for (int i = 0; i < n; i++) {
            ops->insert(test_map, pairs[i].key, safe_strdup(pairs[i].value));
        }
        ops->destroy(test_map);
    }
    end = GetTime();
    metrics->insert_time = (end - start) / repeats;
    fprintf(out, "  1. Insert time: %.6f seconds (avg over %d runs)\n", metrics->insert_time, repeats);

    // ========== СОЗДАЁМ ОСНОВНОЕ ДЕРЕВО ДЛЯ ОСТАЛЬНЫХ ТЕСТОВ ==========
    void* map = ops->create();
    for (int i = 0; i < n; i++) {
        ops->insert(map, pairs[i].key, safe_strdup(pairs[i].value));
    }

    // ========== ВЫСОТА ДО УДАЛЕНИЯ ==========
    metrics->height_before = ops->height(map);
    fprintf(out, "  4. Height before deletion: %zu\n", metrics->height_before);

    // ========== ВРЕМЯ ПОИСКА ==========
    start = GetTime();
    for (int r = 0; r < repeats * 10; r++) {  // поиск быстрее, больше повторений
        for (int i = 0; i < n; i++) {
            ops->find(map, pairs[i].key);
        }
    }
    end = GetTime();
    metrics->search_time = (end - start) / (repeats * 10);
    fprintf(out, "  2. Search time: %.6f seconds (avg over %d runs)\n", metrics->search_time, repeats * 10);

    // ========== ВРЕМЯ УДАЛЕНИЯ ==========
    int remove_count = n / 2;
    start = GetTime();
    for (int r = 0; r < repeats; r++) {
        void* test_map = ops->create();
        // Вставляем данные
        for (int i = 0; i < n; i++) {
            ops->insert(test_map, pairs[i].key, safe_strdup(pairs[i].value));
        }
        // Удаляем каждый второй
        for (int i = 0; i < remove_count; i++) {
            ops->erase(test_map, pairs[i * 2].key);
        }
        ops->destroy(test_map);
    }
    end = GetTime();
    metrics->erase_time = (end - start) / repeats;
    fprintf(out, "  3. Erase time (%d elements): %.6f seconds (avg over %d runs)\n",
        remove_count, metrics->erase_time, repeats);

    // ========== УДАЛЯЕМ ИЗ ОСНОВНОГО ДЕРЕВА ДЛЯ ВЫСОТЫ ПОСЛЕ ==========
    for (int i = 0; i < remove_count; i++) {
        ops->erase(map, pairs[i * 2].key);
    }
    metrics->height_after = ops->height(map);
    fprintf(out, "  4. Height after deletion: %zu\n", metrics->height_after);

    fprintf(out, "\n");

    ops->destroy(map);
}

void write_winner(FILE* out, const char* category, double* values, const char** names, int count, int smaller_is_better) {
    int best_idx = 0;
    for (int i = 1; i < count; i++) {
        if (smaller_is_better) {
            if (values[i] < values[best_idx]) best_idx = i;
        }
        else {
            if (values[i] > values[best_idx]) best_idx = i;
        }
    }

    fprintf(out, "Best in category '%s': %s", category, names[best_idx]);
    if (smaller_is_better) {
        fprintf(out, " (%.6f)\n", values[best_idx]);
    }
    else {
        fprintf(out, "\n");
    }
}

void run_benchmark_from_file(const char* input_file, const char* output_file,
    const TreeImpl** implementations, const char** impl_names, int impl_count) {
    FILE* in = fopen(input_file, "r");
    if (!in) {
        printf("Error: Cannot open input file %s\n", input_file);
        return;
    }

    int capacity = 10000;
    KeyValuePair* pairs = malloc(capacity * sizeof(KeyValuePair));
    int n = 0;

    while (fscanf(in, "%d %63s", &pairs[n].key, pairs[n].value) == 2) {
        n++;
        if (n >= capacity) {
            capacity *= 2;
            pairs = realloc(pairs, capacity * sizeof(KeyValuePair));
        }
    }
    fclose(in);

    if (n == 0) {
        printf("Error: No data in input file\n");
        free(pairs);
        return;
    }

    printf("Loaded %d key-value pairs from %s\n", n, input_file);

    // Сортировка данных (пункт 9)
    qsort(pairs, n, sizeof(KeyValuePair), compare_pairs);
    printf("Data sorted\n");

    FILE* out = fopen(output_file, "w");
    if (!out) {
        printf("Error: Cannot create output file %s\n", output_file);
        free(pairs);
        return;
    }

    fprintf(out, "=== MAP IMPLEMENTATIONS BENCHMARK ===\n");
    fprintf(out, "Data size: %d elements (sorted)\n", n);
    fprintf(out, "Remove count: %d elements\n\n", n / 2);

    TestMetrics* metrics = malloc(impl_count * sizeof(TestMetrics));

    for (int i = 0; i < impl_count; i++) {
        test_map_implementation(implementations[i]->ops, impl_names[i],
            pairs, n, &metrics[i], out);
    }

    fprintf(out, "=== WINNERS ===\n");

    double insert_times[3];
    for (int i = 0; i < impl_count; i++) insert_times[i] = metrics[i].insert_time;
    write_winner(out, "Fastest insert", insert_times, impl_names, impl_count, 1);

    double search_times[3];
    for (int i = 0; i < impl_count; i++) search_times[i] = metrics[i].search_time;
    write_winner(out, "Fastest search", search_times, impl_names, impl_count, 1);

    double erase_times[3];
    for (int i = 0; i < impl_count; i++) erase_times[i] = metrics[i].erase_time;
    write_winner(out, "Fastest erase", erase_times, impl_names, impl_count, 1);

    double heights[3];
    for (int i = 0; i < impl_count; i++) heights[i] = (double)metrics[i].height_before;
    write_winner(out, "Lowest height", heights, impl_names, impl_count, 1);

    fprintf(out, "\n=== SUMMARY TABLE ===\n");
    fprintf(out, "%-12s | %12s | %12s | %12s | %8s | %8s\n",
        "Tree", "Insert(s)", "Search(s)", "Erase(s)", "Height", "H_after");
    fprintf(out, "-------------|--------------|--------------|--------------|----------|----------\n");

    for (int i = 0; i < impl_count; i++) {
        fprintf(out, "%-12s | %12.6f | %12.6f | %12.6f | %8zu | %8zu\n",
            impl_names[i],
            metrics[i].insert_time,
            metrics[i].search_time,
            metrics[i].erase_time,
            metrics[i].height_before,
            metrics[i].height_after);
    }

    fclose(out);
    free(metrics);
    free(pairs);

    printf("Benchmark completed! Results saved to %s\n", output_file);
}