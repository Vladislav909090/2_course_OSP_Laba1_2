#include "plugin_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


static char *g_lib_name = "libvslN3245.so";

#define OPT_BINARY_STRING "bit-seq"


static struct plugin_option g_po_arr[] = {
    {{OPT_BINARY_STRING, required_argument, 0, 0}, "Binary substring"}
};

int plugin_get_info(struct plugin_info* ppi) {
    static char *g_plugin_purpose = "Search for a string in a file represented in binary, decimal, or hexadecimal";
    static char *g_plugin_author = "Vladislav Lozhkin";
    static int g_po_arr_len = sizeof(g_po_arr)/sizeof(g_po_arr[0]);

    if (!ppi) {
        fprintf(stderr, "ERROR: недопустимый аургмент\n");
        return -1;
    }
    
    ppi->plugin_purpose = g_plugin_purpose;
    ppi->plugin_author = g_plugin_author;
    ppi->sup_opts_len = g_po_arr_len;
    ppi->sup_opts = g_po_arr;
    
    return 0;
}


int plugin_process_file(const char *filename, struct option *opts, size_t opts_len) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    const char *value_str = NULL;
    for (size_t i = 0; i < opts_len; i++) {
        if (strcmp(opts[i].name, OPT_BINARY_STRING) == 0) {
            value_str = (const char*)opts[i].flag;
            break;
        }
    }

    if (!filename || !opts || !opts_len || !value_str) {
        errno = EINVAL;
        fclose(file);
        return -1;
    }

    char *DEBUG = getenv("LAB1DEBUG");
    unsigned long long num;
    char *endptr;
    int base = 10; // Default to decimal

    size_t num_bytes;
    if (base == 2) {
        num_bytes = (strlen(value_str) - 2 + 7) / 8;
    } else if (base == 16) {
        num_bytes = (strlen(value_str) - 2) / 2;
    } else {
        num_bytes = 0;
        unsigned long long temp = num;
        while (temp != 0) {
            temp >>= 8;
            num_bytes++;
        }
    }

    if (*endptr != '\0') {
        if (DEBUG) {
            fprintf(stderr, "DEBUG: Неккоректный числовой аргумент: %s\n", value_str);
        }
        errno = EINVAL;
        fclose(file);
        return -1;
    }

    unsigned char le_bytes[num_bytes], be_bytes[num_bytes];
    for (int i = 0; i < num_bytes; i++) {
        le_bytes[i] = (num >> (8 * i)) & 0xFF; // Little-endian
        be_bytes[i] = (num >> (8 * (num_bytes - i - 1))) & 0xFF; // Big-endian
    }

    unsigned char buffer[1024 + num_bytes - 1];
    size_t prevPortion = 0;
    int found = 0;

    while (1) {
        size_t bytesRead = fread(buffer + prevPortion, 1, sizeof(buffer) - prevPortion, file);
        size_t totalBytes = bytesRead + prevPortion;

        if (totalBytes < num_bytes) break;

        for (size_t i = 0; i <= totalBytes - num_bytes; ++i) {
            if (memcmp(&buffer[i], le_bytes, num_bytes) == 0 || memcmp(&buffer[i], be_bytes, num_bytes) == 0) {
                if (DEBUG) {
                    fprintf(stderr, "DEBUG: Found the sequence at position %zu\n", i);
                }
                found = 1;
                break;
            }
        }

        if (found) break;

        if (bytesRead == 0) break;

        prevPortion = num_bytes - 1;
        memmove(buffer, buffer + sizeof(buffer) - prevPortion, prevPortion);
    }

    if (DEBUG && !found) {
        fprintf(stderr, "DEBUG: Последовательность не найдена\n");
    }

    fclose(file);
    return found ? 0 : 1;\
}