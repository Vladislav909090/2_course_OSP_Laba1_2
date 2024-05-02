// #define _XOPEN_SOURCE 700 // Для активации макроса DT_REG и других функций POSIX

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <dirent.h>
#include <string.h>

#include "plugin_api.h"

// Загрузка информации о плагине
int load_plugin(const char *plugin_path, struct plugin_info *info, void **handle) {
    *handle = dlopen(plugin_path, RTLD_LAZY);
    if (!*handle) {
        fprintf(stderr, "Could not load plugin: %s\n", dlerror());
        return -1;
    }

    int (*plugin_get_info)(struct plugin_info *) = (int (*)(struct plugin_info *))dlsym(*handle, "plugin_get_info");
    if (!plugin_get_info) {
        fprintf(stderr, "Could not find plugin_get_info function: %s\n", dlerror());
        dlclose(*handle);
        return -1;
    }

    if (plugin_get_info(info) != 0) {
        fprintf(stderr, "Plugin get info failed.\n");
        dlclose(*handle);
        return -1;
    }

    return 0;
}

// Проход по файлам в директории и применение плагина
void process_directory(const char *dir_path, void *plugin_handle, struct option *plugin_options, size_t options_len) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("Failed to open directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {  // Проверка, что это обычный файл
            char file_path[1024];
            snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);
            printf("Processing file: %s\n", file_path);

            // Вызов функции обработки файла плагина
            int (*plugin_process)(const char *, struct option *, size_t) = (int (*)(const char *, struct option *, size_t))dlsym(plugin_handle, "plugin_process_file");
            if (plugin_process) {
                int result = plugin_process(file_path, plugin_options, options_len);
                if (result == 0) {
                    printf("File '%s' matches criteria.\n", file_path);
                } else {
                    printf("File '%s' does not match criteria.\n", file_path);
                }
            } else {
                fprintf(stderr, "Could not find plugin_process_file function: %s\n", dlerror());
            }
        }
    }

    closedir(dir);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <directory_path> <plugin_path> [--plugin_option1=value1 ...]\n", argv[0]);
        return 1;
    }

    const char *dir_path = argv[1];
    const char *plugin_path = argv[2];

    // Загрузка плагина
    struct plugin_info info;
    void *plugin_handle = NULL;
    if (load_plugin(plugin_path, &info, &plugin_handle) != 0) {
        return 1;
    }

    // Сбор опций плагина
    struct option *plugin_options = calloc(info.options_count + 1, sizeof(struct option));
    for (size_t i = 0; i < info.options_count; i++) {
        plugin_options[i] = info.options[i].opt;
    }

    // Парсинг опций командной строки, начиная с 3-го аргумента
    int opt;
    while ((opt = getopt(argc - 2, argv + 2, "")) != -1) {
        for (size_t i = 0; i < info.options_count; i++) {
            if (plugin_options[i].val == opt) {
                plugin_options[i].flag = (int *)optarg;  // Просто пример, обычно потребуется доработка
                break;
            }
        }
    }

    // Обработка файлов в директории с помощью плагина
    process_directory(dir_path, plugin_handle, plugin_options, info.options_count);

    // Очистка
    if (plugin_handle) {
        dlclose(plugin_handle);
    }
    free(plugin_options);

    return 0;
}