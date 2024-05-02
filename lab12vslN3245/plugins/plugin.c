#include "plugin_api.h"
#include <stdio.h>
#include <string.h>

// Глобальные переменные для хранения информации о плагине
static struct plugin_option options[] = {
    {{"substring", required_argument, NULL, 's'}, "Search substring in files"}
};

static struct plugin_info info = {
    "Substring Search Plugin",
    "Your Name",
    1,
    options
};

int plugin_get_info(struct plugin_info *info_ptr) {
    if (info_ptr) {
        *info_ptr = info;
        return 0;
    }
    return -1;
}

int plugin_process_file(const char *filename, struct option *opts, size_t opts_len) {
    char buffer[1024];
    FILE *file = fopen(filename, "r");
    if (!file) return -1;

    const char *substring = NULL;
    for (size_t i = 0; i < opts_len; i++) {
        if (strcmp(opts[i].name, "substring") == 0) {
            substring = (const char*)opts[i].flag;
            break;
        }
    }

    if (!substring) {
        fclose(file);
        return -1;
    }

    int found = 0;
    while (fgets(buffer, sizeof(buffer), file)) {
        if (strstr(buffer, substring)) {
            found = 1;
            break;
        }
    }

    fclose(file);
    return found ? 0 : 1;
}