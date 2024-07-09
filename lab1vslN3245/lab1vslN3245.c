#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include <ftw.h>
#include "plugin_api.h"
#include <getopt.h>

typedef struct {
    struct plugin_info info;
    char *filename;
    void *handle;
    struct option *long_opts;
} loaded_plugin;

loaded_plugin *loaded_plugins = NULL;
size_t loaded_plugin_count = 0;

void free_loaded_plugins() {
    for (size_t i = 0; i < loaded_plugin_count; i++) {
        if (loaded_plugins[i].handle) {
            dlclose(loaded_plugins[i].handle);
        }
        free(loaded_plugins[i].filename);
        free(loaded_plugins[i].long_opts);
    }
    free(loaded_plugins);
    loaded_plugins = NULL;
    loaded_plugin_count = 0;
}

void load_plugins(char *plugin_dir) {
    DIR *dir = opendir(plugin_dir);
    if (!dir) {
        perror("Failed to open plugin directory");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".so")) {
            char lib_path[1024];
            snprintf(lib_path, sizeof(lib_path), "%s/%s", plugin_dir, entry->d_name);
            void *handle = dlopen(lib_path, RTLD_LAZY);
            if (!handle) {
                fprintf(stderr, "ERROR: dlopen() failed for %s: %s\n", lib_path, dlerror());
                continue;
            }

            loaded_plugin *new_ptr = realloc(loaded_plugins, (loaded_plugin_count + 1) * sizeof(loaded_plugin));
            if (!new_ptr) {
                perror("Failed to allocate memory for plugins");
                dlclose(handle);
                continue;
            }
            loaded_plugins = new_ptr;
            loaded_plugin *new_plugin = &loaded_plugins[loaded_plugin_count++];
            new_plugin->handle = handle;
            new_plugin->filename = strdup(entry->d_name);

            void *func = dlsym(handle, "plugin_get_info");
            if (func) {
                typedef int (*pgi_func_t)(struct plugin_info*);
                pgi_func_t pgi_func = (pgi_func_t)func;
                if (pgi_func(&new_plugin->info) != 0) {
                    fprintf(stderr, "ERROR: plugin_get_info() failed for %s\n", lib_path);
                    dlclose(handle);
                    free(new_plugin->filename);
                    loaded_plugin_count--;
                    continue;
                }

                new_plugin->long_opts = malloc((new_plugin->info.sup_opts_len + 1) * sizeof(struct option));
                for (size_t i = 0; i < new_plugin->info.sup_opts_len; i++) {
                    new_plugin->long_opts[i].name = new_plugin->info.sup_opts[i].opt.name;
                    new_plugin->long_opts[i].has_arg = new_plugin->info.sup_opts[i].opt.has_arg;
                    new_plugin->long_opts[i].flag = new_plugin->info.sup_opts[i].opt.flag;
                    new_plugin->long_opts[i].val = new_plugin->info.sup_opts[i].opt.val;
                }
                // Terminate the array
                new_plugin->long_opts[new_plugin->info.sup_opts_len].name = NULL;
                new_plugin->long_opts[new_plugin->info.sup_opts_len].has_arg = 0;
                new_plugin->long_opts[new_plugin->info.sup_opts_len].flag = 0;
                new_plugin->long_opts[new_plugin->info.sup_opts_len].val = 0;
            } else {
                fprintf(stderr, "ERROR: dlsym() failed for %s: %s\n", lib_path, dlerror());
                dlclose(handle);
                free(new_plugin->filename);
                loaded_plugin_count--;
            }
        }
    }
    closedir(dir);
}

void print_version() {
    printf("Program version: 1.0\n"
           "Author: Lozhkin Vladislav\n"
           "Group: N3245\n"
           "Lab version: 5\n");
}

void print_help(char *prog_name) {
    printf("Usage: %s [options] /path/to/directory\n"
           "Options:\n"
           "  -P, --plugin-dir DIR   Specify the directory where plugins are located.\n"
           "  -v, --version          Display version and author information.\n"
           "  -h, --help             Show this help message and load plugins information.\n",
           prog_name);

    if (loaded_plugin_count > 0) {
        printf("Loaded plugins:\n");
        for (size_t i = 0; i < loaded_plugin_count; i++) {
            printf("  Plugin: %s\n", loaded_plugins[i].filename);
            printf("  Description: %s\n", loaded_plugins[i].info.plugin_purpose);
            printf("  Supported options:\n");
            for (size_t j = 0; j < loaded_plugins[i].info.sup_opts_len; j++) {
                printf("    --%s\n", loaded_plugins[i].info.sup_opts[j].opt.name);
            }
        }
    }
}

void add_plugin_options(struct option **long_options, int *option_count) {
    int total_options = *option_count;

    for (size_t i = 0; i < loaded_plugin_count; i++) {
        total_options += loaded_plugins[i].info.sup_opts_len;
    }

    struct option *new_long_options = realloc(*long_options, (total_options + 1) * sizeof(struct option));
    if (!new_long_options) {
        perror("Failed to allocate memory for plugin options");
        exit(EXIT_FAILURE);
    }

    int index = *option_count;
    for (size_t i = 0; i < loaded_plugin_count; i++) {
        for (size_t j = 0; j < loaded_plugins[i].info.sup_opts_len; j++) {
            new_long_options[index++] = loaded_plugins[i].long_opts[j];
        }
    }
    new_long_options[index].name = NULL;
    new_long_options[index].has_arg = 0;
    new_long_options[index].flag = NULL;
    new_long_options[index].val = 0;

    *long_options = new_long_options;
    *option_count = index;
}

int file_process(const char *fpath, const struct stat *sb, int typeflag) {
    if (typeflag == FTW_F) {
        printf("Processing file: %s\n", fpath);
        for (size_t i = 0; i < loaded_plugin_count; i++) {
            typedef int (*process_file_func_t)(const char *, struct option *, size_t);
            void *func = dlsym(loaded_plugins[i].handle, "plugin_process_file");
            if (func) {
                process_file_func_t process_func = (process_file_func_t)func;
                int result = process_func(fpath, loaded_plugins[i].long_opts, loaded_plugins[i].info.sup_opts_len);
                printf("Plugin %s %s the file %s\n", loaded_plugins[i].filename, result ? "accepted" : "rejected", fpath);
            }
        }
    }
    return 0; // continue walking
}

int main(int argc, char *argv[]) {
    struct option *long_options = malloc(4 * sizeof(struct option));
    if (!long_options) {
        perror("Failed to allocate memory for long_options");
        exit(EXIT_FAILURE);
    }

    long_options[0] = (struct option){"plugin-dir", required_argument, NULL, 'P'};
    long_options[1] = (struct option){"version", no_argument, NULL, 'v'};
    long_options[2] = (struct option){"help", no_argument, NULL, 'h'};
    long_options[3] = (struct option){NULL, 0, NULL, 0};

    char *plugin_dir = ".";
    int opt, option_index = 0;
    int option_count = 3;  // Количество базовых опций

    while ((opt = getopt_long(argc, argv, "P:vh", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'P':
            plugin_dir = optarg;
            break;
        case 'v':
            print_version();
            free(long_options);
            return 0;
        case 'h':
            load_plugins(plugin_dir);
            print_help(argv[0]);
            free_loaded_plugins();
            free(long_options);
            return 0;
        default:
            fprintf(stderr, "Unknown option. Use -h for help.\n");
            free(long_options);
            exit(EXIT_FAILURE);
        }
    }

    load_plugins(plugin_dir);
    add_plugin_options(&long_options, &option_count);

    optind = 1; // reset optind to re-parse with new options
    while ((opt = getopt_long(argc, argv, "P:vh", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'P':
            break;
        case 'v':
            break;
        case 'h':
            break;
        default:
            // Process plugin-specific options
            for (size_t i = 0; i < loaded_plugin_count; i++) {
                for (size_t j = 0; j < loaded_plugins[i].info.sup_opts_len; j++) {
                    if (loaded_plugins[i].long_opts[j].val == opt) {
                        // Предполагаем, что каждый плагин реализует функцию plugin_process_option
                        typedef int (*process_option_func_t)(int, const char*);
                        process_option_func_t process_option_func = (process_option_func_t)dlsym(loaded_plugins[i].handle, "plugin_process_option");
                        if (process_option_func) {
                            int result = process_option_func(opt, optarg);
                            if (result != 0) {
                                fprintf(stderr, "Error processing option %s by plugin %s\n", loaded_plugins[i].long_opts[j].name, loaded_plugins[i].filename);
                            }
                        } else {
                            fprintf(stderr, "Error: failed to find handler for option %s in plugin %s: %s\n", loaded_plugins[i].long_opts[j].name, loaded_plugins[i].filename, dlerror());
                        }
                        break; // Предполагаем, что каждая опция обрабатывается только одним плагином.
                    }
                }
            }
            break;
        }
    }


    if (optind >= argc) {
        fprintf(stderr, "Expected argument after options\n");
        print_help(argv[0]);
        free_loaded_plugins();
        free(long_options);
        exit(EXIT_FAILURE);
    }

    char *dir_to_process = argv[optind];

    if (ftw(dir_to_process, file_process, 20) == -1) {
        perror("ftw");
        free_loaded_plugins();
        free(long_options);
        exit(EXIT_FAILURE);
    }

    free_loaded_plugins();
    free(long_options);
    return 0;
}
