#include "plugin_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

// Имя библиотеки
static char *g_lib_name = "libvslN3245.so";

// Определение строки опции
#define OPT_BINARY_STRING "bit-seq"

// Массив поддерживаемых опций плагина
static struct plugin_option g_po_arr[] = {
    {{OPT_BINARY_STRING, required_argument, 0, 0}, "Binary substring"}
};

// Функция для получения информации о плагине
int plugin_get_info(struct plugin_info* ppi) {
    // Назначение и автор плагина
    static char *g_plugin_purpose = "Search for a string in a file represented in binary, decimal, or hexadecimal";
    static char *g_plugin_author = "Vladislav Lozhkin";
    // Длина массива опций
    static int g_po_arr_len = sizeof(g_po_arr)/sizeof(g_po_arr[0]);

    // Проверка на допустимость указателя ppi
    if (!ppi) {
        fprintf(stderr, "ERROR: недопустимый аргумент\n");
        return -1;
    }
    
    // Заполнение структуры информацией о плагине
    ppi->plugin_purpose = g_plugin_purpose;
    ppi->plugin_author = g_plugin_author;
    ppi->sup_opts_len = g_po_arr_len;
    ppi->sup_opts = g_po_arr;
    
    return 0;
}

// Функция для обработки файла с учетом опций
int plugin_process_file(const char *filename, struct option *opts, size_t opts_len) {
    // Проверка допустимости входных параметров
    if (!filename || !opts || opts_len == 0) {
        errno = EINVAL;
        return -1;
    }
    
    // Открытие файла для чтения в бинарном режиме
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    // Поиск значения опции среди переданных
    const char *value_str = NULL;
    for (size_t i = 0; i < opts_len; i++) {
        if (strcmp(opts[i].name, OPT_BINARY_STRING) == 0) {
            value_str = (const char*)opts[i].flag;
            break;
        }
    }

    // Проверка наличия значения опции
    if (!value_str) {
        fprintf(stderr, "ERROR: Option value is missing\n");
        fclose(file);
        return -1;
    }

    // Преобразование строки в число с автоматическим определением базы
    char *endptr;
    errno = 0;
    unsigned long long num = strtoull(value_str, &endptr, 0);
    // Проверка на ошибки преобразования
    if (*endptr != '\0' || errno == ERANGE || errno != 0) {
        fprintf(stderr, "ERROR: Invalid numeric argument '%s'\n", value_str);
        fclose(file);
        return -1;
    }
    
    // Определение длины числа в байтах
    char *DEBUG = getenv("LAB1DEBUG");
    size_t num_bytes = 0;
    while (num != 0) {
        num >>= 8;
        num_bytes++;
    }

    // Массивы для хранения числа в формате little-endian и big-endian
    unsigned char le_bytes[num_bytes], be_bytes[num_bytes];
    num = strtoull(value_str, &endptr, 0); // Повторное чтение числа
    for (size_t i = 0; i < num_bytes; i++) {
        le_bytes[i] = (num >> (8 * i)) & 0xFF;
        be_bytes[i] = (num >> (8 * (num_bytes - i - 1))) & 0xFF;
    }

    // Буфер для чтения файла
    unsigned char buffer[1024];
    size_t bytesRead, totalBytes = 0;

    // Чтение файла и поиск последовательности
    while ((bytesRead = fread(buffer + totalBytes, 1, sizeof(buffer) - totalBytes, file)) > 0 || totalBytes >= num_bytes) {
        totalBytes += bytesRead;

        // Поиск последовательности в буфере
        for (size_t i = 0; i <= totalBytes - num_bytes; ++i) {
            if (memcmp(&buffer[i], le_bytes, num_bytes) == 0 || memcmp(&buffer[i], be_bytes, num_bytes) == 0) {
                if (DEBUG) {
                    fprintf(stderr, "DEBUG: Found the sequence at position %zu\n", i);
                }
                fclose(file);
                return 0;
            }
        }

        // Перемещение оставшихся байтов в начало буфера
        if (totalBytes > num_bytes) {
            memmove(buffer, buffer + totalBytes - num_bytes, num_bytes);
            totalBytes = num_bytes;
        }
    }

    // Если последовательность не найдена
    if (DEBUG) {
        fprintf(stderr, "DEBUG: Sequence not found\n");
    }

    fclose(file);
    return 1;
}
