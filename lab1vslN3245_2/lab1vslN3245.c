#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ftw.h>
#include <sys/param.h>      // для MIN()
#include <getopt.h>
#include <dlfcn.h>
#include "plugin_api.h"

// Объявление функций
int open_func(const char *fpath, const struct stat *sb, int typeflag);
void open_dyn_libs(const char *dir);
void optparse(int argc, char *argv[]);
void walk_dir(const char *dir);

// Указатели на функции
typedef int (*ppf_func_t)(const char*, struct option*, size_t);
typedef int (*pgi_func_t)(struct plugin_info*);

// Структура для хранения информации о динамических библиотеках
typedef struct {
    void* lib;                  // Дескриптор загруженной библиотеки
    struct plugin_info pi;      // Информация о плагине
    ppf_func_t ppf;             // Указатель на функцию обработки файлов плагина
    struct option* in_opts;     // Опции, предоставленные плагину
    size_t in_opts_len;         // Количество предоставленных опций
} dynamic_lib; 

// Глобальные переменные для динамических библиотек
dynamic_lib *plugins = NULL;    // Массив загруженных плагинов
int plug_cnt = 0;               // Количество загруженных плагинов
int or = 0, not = 0;            // Флаги логических операций
int found_opts = 0, got_opts = 0;// Количество найденных и полученных опций

// Реализация функции open_func
int open_func(const char *fpath, const struct stat *sb, int typeflag) {
    // Проверка использования всех параметров для предотвращения предупреждений компилятора
    (void) sb;
    (void) typeflag;

    // Проверка корректности пути к файлу
    if (!fpath) {
        fprintf(stderr, "Invalid file path\n");
        return 0;
    }

    // Проверка, является ли файл разделяемой библиотекой
    if (typeflag == FTW_F && strstr(fpath, ".so") != NULL) {
        // Открытие разделяемой библиотеки
        void *library = dlopen(fpath, RTLD_LAZY);
        if (!library) {
            // В случае ошибки открытия вывод сообщения и продолжение поиска
            fprintf(stderr, "dlopen() failed for %s: %s\n", fpath, dlerror());
            return 0;
        } else {
            // Получение указателей на функции плагина
            void* pi_f = dlsym(library, "plugin_get_info");
            if (!pi_f) {
                // В случае отсутствия функции plugin_get_info вывод сообщения об ошибке
                fprintf(stderr, "dlsym() failed for plugin_get_info: %s\n", dlerror());
                dlclose(library);
                return 0;
            } 
            
            void* pf_f = dlsym(library, "plugin_process_file");
            if (!pf_f) {
                // В случае отсутствия функции plugin_process_file вывод сообщения об ошибке
                fprintf(stderr, "dlsym() failed for plugin_process_file: %s\n", dlerror());
                dlclose(library);
                return 0;
            }

            // Вызов функции plugin_get_info для получения информации о плагине
            struct plugin_info pi = {0};
            pgi_func_t pgi = (pgi_func_t)pi_f;
            int tmp = pgi(&pi);
            if (tmp == -1) {
                // В случае ошибки в plugin_get_info вывод сообщения об ошибке
                fprintf(stderr, "Error in plugin_get_info\n");
                dlclose(library);
                return 0;
            }

            // Выделение памяти для массива плагинов и добавление информации о плагине
            plugins = realloc(plugins, sizeof(dynamic_lib) * (plug_cnt + 1));
            plugins[plug_cnt].pi = pi;
            plugins[plug_cnt].ppf = (ppf_func_t)pf_f;
            plugins[plug_cnt].lib = library;
            plugins[plug_cnt].in_opts = NULL;
            plugins[plug_cnt].in_opts_len = 0;
            plug_cnt++;
            found_opts += pi.sup_opts_len;
        }
    }
    
    return 0; // Возвращение 0 для продолжения обхода каталога
}

// Главная функция
int main(int argc, char *argv[]) {
    open_dyn_libs("./"); // Открытие динамических библиотек в текущем каталоге
    optparse(argc, argv); // Разбор параметров командной строки

    // Проверка наличия опций. Если опции не найдены, вывод сообщения и завершение
    if (got_opts == 0) {
        printf("No options found. Use -h for help\n");

        // Освобождение выделенной памяти и закрытие открытых библиотек
        if (plugins) {
            for (int i = 0; i < plug_cnt; i++) {
                if (plugins[i].in_opts) free(plugins[i].in_opts);
                dlclose(plugins[i].lib);
            }
            free(plugins);
        }
        exit(EXIT_FAILURE);
    }

    // Обход каталога, указанного в последнем аргументе командной строки
    walk_dir(argv[argc-1]);

    // Освобождение выделенной памяти и закрытие открытых библиотек
    if (plugins) {
        for (int i = 0; i < plug_cnt; i++) {
            if (plugins[i].in_opts) free(plugins[i].in_opts);
            dlclose(plugins[i].lib);
        }
        free(plugins);
    }   

    return EXIT_SUCCESS; // Возвращение кода успешного завершения
}

// Функция открытия динамических библиотек
void open_dyn_libs(const char *dir){
    int res = ftw(dir, open_func, 10); // Открытие плагинов
    if (res < 0) {
        fprintf(stderr, "ftw() failed: %s\n", strerror(errno));
    }
}

void display_usage(const char *program_name) {
    printf("\nUsage: %s <options> <dir>\n", program_name);
    printf("<dir> - Directory to search\n");
    printf("\nAvailable options:\n");
    printf("  -P <dir>    Change plugin directory\n");
    printf("  -h          Display this help message\n");
    printf("  -A          Use 'and' logical operation\n");
    printf("  -O          Use 'or' logical operation\n");
    printf("  -N          Use 'not' logical operation\n");
}

void display_plugins_info() {
    printf("\nPlugin Information:\n");
    for(int i = 0; i < plug_cnt; i++) {
        printf("Plugin purpose: %s\n", plugins[i].pi.plugin_purpose);
        for(size_t j = 0; j < plugins[i].pi.sup_opts_len; j++) {
            printf("  --%s     %s\n", plugins[i].pi.sup_opts[j].opt.name, plugins[i].pi.sup_opts[j].opt_descr);
        }
        printf("\n");
    }
}

void display_version() {
    printf("\n");
    printf("Author: Vladislav Lozhkin Sergeevich\n");
    printf("Group: N3245\n");
    printf("Program version: 1.0\n");
    printf("\n");
}

void optparse(int argc, char *argv[]) {
    // Выделение памяти для структур опций
    struct option *long_options = calloc(found_opts + 1, sizeof(struct option));
    int copied = 0;

    // Копирование опций из всех плагинов в общий список опций
    for (int i = 0; i < plug_cnt; i++) {
        for (size_t j = 0; j < plugins[i].pi.sup_opts_len; j++) {
            long_options[copied] = plugins[i].pi.sup_opts[j].opt;
            copied++;
        }
    }

    int option_index = 0;
    int choice;

    // Разбор опций
    while ((choice = getopt_long(argc, argv, "vhP:OAN", long_options, &option_index)) != -1) {
        switch (choice) {
            case 0:
                // Обработка пользовательских опций
                for (int i = 0; i < plug_cnt; i++) {
                    for (size_t j = 0; j < plugins[i].pi.sup_opts_len; j++) {
                        if (strcmp(long_options[option_index].name, plugins[i].pi.sup_opts[j].opt.name) == 0) {
                            // Расширение массива опций для хранения опции
                            plugins[i].in_opts = realloc(plugins[i].in_opts, (plugins[i].in_opts_len + 1) * sizeof(struct option));
                            plugins[i].in_opts[plugins[i].in_opts_len] = long_options[option_index];

                            // Установка флага, если опция имеет аргумент
                            if (plugins[i].in_opts[plugins[i].in_opts_len].has_arg != 0)
                                plugins[i].in_opts[plugins[i].in_opts_len].flag = (int *)optarg;

                            plugins[i].in_opts_len++;
                            got_opts++;
                        }
                    }
                }
                break;
            case 'h':
                display_usage(argv[0]);
                display_plugins_info();
                // Освобождение памяти и завершение работы
                if (plugins) {
                    for (int i = 0; i < plug_cnt; i++) {
                        if (plugins[i].in_opts) free(plugins[i].in_opts);
                        dlclose(plugins[i].lib);
                    }
                    free(plugins);
                }
                free(long_options);
                exit(EXIT_SUCCESS);
            case 'v':
                display_version();
                // Освобождение памяти и завершение работы
                if (plugins) {
                    for (int i = 0; i < plug_cnt; i++) {
                        if (plugins[i].in_opts) free(plugins[i].in_opts);
                        dlclose(plugins[i].lib);
                    }
                    free(plugins);
                }
                free(long_options);
                exit(EXIT_SUCCESS);
            case 'P':
                // Изменение текущего плагина
                if (got_opts > 0) {
                    fprintf(stderr, "-P must be before plugin opts!\n");
                    free(long_options); // Если опция была найдена до -P, это может вызвать проблемы
                    got_opts = 0;
                    return;
                }

                // Закрытие текущих библиотек и освобождение памяти
                for (int i = 0; i < plug_cnt; i++) {
                    dlclose(plugins[i].lib);
                }
                free(plugins);
                plugins = NULL;
                plug_cnt = 0;
                found_opts = 0;

                // Вывод отладочной информации и открытие новых плагинов
                if (getenv("LAB1DEBUG") != NULL) fprintf(stderr, "New lib path: %s\n", optarg);
                free(long_options);
                open_dyn_libs(optarg);
                long_options = calloc(found_opts + 1, sizeof(struct option));
                copied = 0;
                for (int i = 0; i < plug_cnt; i++) {
                    for (size_t j = 0; j < plugins[i].pi.sup_opts_len; j++) {
                        long_options[copied] = plugins[i].pi.sup_opts[j].opt;
                        copied++;
                    }
                }
                break;
            case 'O':
                or = 1;
                break;
            case 'A':
                or = 0;
                break;
            case 'N':
                not = 1;
                break;
            case '?':
                break;
        }
    }
    free(long_options);
}

// Функция для печати информации о найденных файлах
void print_entry(int type, const char *path) {
    // Пропуск записей каталога и нерегулярных файлов
    if (!strcmp(path, ".") || !strcmp(path, "..") || type != FTW_F)
        return;

    int cnt = 0;
    int cnt_success = 0;
    
    // Итерация по всем плагинам для обработки файла с соответствующими опциями
    for(int i = 0; i < plug_cnt; i++){
        // Пропуск плагинов без установленных опций
        if(plugins[i].in_opts_len > 0){
            // Вызов функции обработки плагина с указанными опциями
            int tmp = plugins[i].ppf(path, plugins[i].in_opts, plugins[i].in_opts_len);
            
            // Обработка ошибок, если есть
            if(tmp == -1){
                fprintf(stderr, "Error in plugin! %s", strerror(errno));
                // Сброс опций при возникновении ошибки
                if(errno == EINVAL || errno == ERANGE) 
                    plugins[i].in_opts_len = 0;
            } else if (tmp == 0) 
                cnt++;
            cnt_success++;
        }
    }
    
    // Проверка выполнения условий 'or' и 'not'
    if((not && or && (cnt == 0)) || (not && !or && (cnt != cnt_success))){
        // Печать пути найденного файла
        printf("Found file: %s\n", path);
    } else if((!not && or && (cnt > 0)) || (!not && !or && (cnt == cnt_success))){
        // Печать пути найденного файла
        printf("Found file: %s\n", path);
    }
    return;
} 

// Функция обхода каталогов
int walk_func(const char *fpath, const struct stat *sb, int typeflag) {
    if(!sb) return -1;
    print_entry( typeflag, fpath); 
   
    return 0;
}

// Функция для обхода каталогов
void walk_dir(const char *dir) {
    int res = ftw(dir, walk_func, 10);   
    if (res < 0) {
        fprintf(stderr, "ftw() failed: %s\n", strerror(errno));
    }
}
