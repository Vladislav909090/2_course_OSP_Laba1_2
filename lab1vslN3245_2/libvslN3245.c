#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <string.h>

#include "plugin_api.h"

// Назначение плагина и информация об авторе
static char *g_purpose = "Проверка, содержит ли файл указанную битовую последовательность";
static char *g_author = "Lozhkin Vladislav";

// Поддерживаемые опции плагина
static struct plugin_option g_options[] = {
    {
        {"bit-seq", required_argument, 0, 0},
        "Битовая последовательность для поиска"
    }
};

// Количество поддерживаемых опций плагина
static int g_options_len = sizeof(g_options) / sizeof(g_options[0]);

// Функция для получения информации о плагине
int plugin_get_info(struct plugin_info *ppi)
{
    // Проверка корректности аргумента
    if (!ppi)
    {
        fprintf(stderr, "ERROR: Invalid argument\n");
        return -1;
    }

    // Назначение цели плагина, автора и поддерживаемых опций
    ppi->plugin_purpose = g_purpose;
    ppi->plugin_author = g_author;
    ppi->sup_opts_len = g_options_len;
    ppi->sup_opts = g_options;

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
    const char *bitseq_value_str = NULL;
    for (size_t i = 0; i < opts_len; i++) {
        if (strcmp(opts[i].name, "bit-seq") == 0) {
            bitseq_value_str = (const char*)opts[i].flag;
            break;
        }
    }

    // Проверка наличия значения опции
    if (!bitseq_value_str) {
        fprintf(stderr, "ERROR: Option value is missing\n");
        fclose(file);
        return -1;
    }

    // Преобразование строки в число с поддержкой двоичного, десятичного и шестнадцатеричного форматов
    unsigned long long bit_seq = 0;
    size_t num_bits = 0;
    if (strncmp(bitseq_value_str, "0b", 2) == 0) {
        // Двоичный формат
        for (size_t i = 2; i < strlen(bitseq_value_str); i++) {
            if (bitseq_value_str[i] == '0' || bitseq_value_str[i] == '1') {
                bit_seq = (bit_seq << 1) | (bitseq_value_str[i] - '0');
                num_bits++;
            } else {
                fprintf(stderr, "ERROR: Invalid binary digit '%c'\n", bitseq_value_str[i]);
                fclose(file);
                return -1;
            }
        }
    } else {
        // Десятичный и шестнадцатеричный форматы
        char *endptr;
        bit_seq = strtoull(bitseq_value_str, &endptr, 0);
        if (*endptr != '\0' || errno == ERANGE || errno != 0) {
            fprintf(stderr, "ERROR: Invalid numeric argument '%s'\n", bitseq_value_str);
            fclose(file);
            return -1;
        }

        // Определение длины числа в битах
        unsigned long long temp = bit_seq;
        while (temp != 0) {
            temp >>= 1;
            num_bits++;
        }
    }

    // Буфер для чтения файла
    unsigned char buffer[1024];
    size_t bytesRead, totalBytes = 0;

    // Чтение файла и поиск последовательности
    while ((bytesRead = fread(buffer + totalBytes, 1, sizeof(buffer) - totalBytes, file)) > 0 || totalBytes >= (num_bits + 7) / 8) {
        totalBytes += bytesRead;

        // Поиск битовой последовательности в буфере
        for (size_t i = 0; i <= totalBytes * 8 - num_bits; ++i) {
            unsigned long long value = 0;
            for (size_t j = 0; j < num_bits; ++j) {
                value <<= 1;
                value |= (buffer[(i + j) / 8] >> (7 - (i + j) % 8)) & 1;
            }
            if (value == bit_seq) {
                if (getenv("LAB1DEBUG") != NULL) {
                    fprintf(stderr, "DEBUG: Found the bit sequence at byte position %zu\n", i / 8);
                }
                fclose(file);
                return 0;
            }
        }

        // Перемещение оставшихся байтов в начало буфера
        if (totalBytes > (num_bits + 7) / 8) {
            memmove(buffer, buffer + totalBytes - (num_bits + 7) / 8, (num_bits + 7) / 8);
            totalBytes = (num_bits + 7) / 8;
        }
    }

    // Если последовательность не найдена
    if (getenv("LAB1DEBUG") != NULL) {
        fprintf(stderr, "DEBUG: Bit sequence not found\n");
    }

    fclose(file);
    return 1;
}
