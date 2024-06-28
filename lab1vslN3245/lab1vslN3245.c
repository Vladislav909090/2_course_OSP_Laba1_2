#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>   // Для работы с директориями
#include <dlfcn.h>
#include "plugin_api.h"


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [-P /path/to/dir_with_plugins] [options] /path/to/file_to_search\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *current_dir = "."; // Текущая директория
    char *file_name = argv[argc - 1]; // Предполагаем, что файл всегда последний аргумент
    DIR *dir;
    struct dirent *entry;

    dir = opendir(current_dir);
    if (!dir) {
        perror("Failed to open directory");
        return EXIT_FAILURE;
    }


        while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".so")) {
            char lib_path[1024];
            snprintf(lib_path, sizeof(lib_path), "%s/%s", current_dir, entry->d_name); // строка полного пути до каждого файла
            void *dl = dlopen(lib_path, RTLD_LAZY); // попытка открытия файлов
            if (!dl) {
                fprintf(stderr, "ERROR: dlopen() failed for %s: %s\n", lib_path, dlerror());
                continue;
            }
            void *func = dlsym(dl, "plugin_get_info");
            if (func) {
                struct plugin_info pi = {0};
                typedef int (*pgi_func_t)(struct plugin_info*);
                pgi_func_t pgi_func = (pgi_func_t)func;
                if (pgi_func(&pi) == 0) {
                    printf("Loaded: %s\n", lib_path);
                    printf("Plugin purpose: %s\n", pi.plugin_purpose);
                    printf("Plugin author: %s\n", pi.plugin_author);
                    for (int i = 0; i < pi.sup_opts_len; i++) {
                        printf("Supported option: --%s\n", pi.sup_opts[i].opt.name);
                    }
                }
            }
            dlclose(dl);
        }
    }
    closedir(dir);
}