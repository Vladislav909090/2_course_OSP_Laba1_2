#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include <getopt.h>

// Структура для описания опций плагина
struct plugin_option {
    struct option opt;     // опция для getopt_long
    const char *descr; // описание опции
};

// Структура информации о плагине
struct plugin_info {
    const char *plugin_purpose; // имя плагина
    const char *plugin_author; // автор плагина
    size_t sup_opts_len;    // количество опций
    struct plugin_option *sup_opts; // массив опций
};

int plugin_get_info(struct plugin_info *ppi);

// Функция, которая будет вызываться для каждого файла
int plugin_process_file(const char *fname, struct option in_opts[], size_t in_opts_len);

#endif