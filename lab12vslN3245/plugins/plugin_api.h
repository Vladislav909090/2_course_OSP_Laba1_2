#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include <getopt.h>

// Структура для описания опций плагина
struct plugin_option {
    struct option opt;     // опция для getopt_long
    const char *description; // описание опции
};

// Структура информации о плагине
struct plugin_info {
    const char *plugin_name; // имя плагина
    const char *plugin_author; // автор плагина
    size_t options_count;    // количество опций
    struct plugin_option *options; // массив опций
};

int plugin_get_info(struct plugin_info *info);

// Функция, которая будет вызываться для каждого файла
int plugin_process_file(const char *filename, struct option *options, size_t options_len);

#endif