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
        fprintf(stderr, "ERROR: недопустимый аргумент\n");
        return -1;
    }
    
    ppi->plugin_purpose = g_plugin_purpose;
    ppi->plugin_author = g_plugin_author;
    ppi->sup_opts_len = g_po_arr_len;
    ppi->sup_opts = g_po_arr;
    
    return 0;
}


int plugin_process_file(const char *filename, struct option *opts, size_t opts_len) {
    if (!filename || !opts || opts_len == 0) {
        errno = EINVAL;
        return -1;
    }
    
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

    if (!value_str) {
        fprintf(stderr, "ERROR: Option value is missing\n");
        fclose(file);
        return -1;
    }

    char *endptr;
    errno = 0;
    unsigned long long num = strtoull(value_str, &endptr, 0); // Auto-detect the base
    if (*endptr != '\0' || errno == ERANGE) {
        fprintf(stderr, "ERROR: Invalid numeric argument '%s'\n", value_str);
        fclose(file);
        return -1;
    }
    
    char *DEBUG = getenv("LAB1DEBUG");
    size_t num_bytes = 0;
    while (num != 0) {
        num >>= 8;
        num_bytes++;
    }

    unsigned char le_bytes[num_bytes], be_bytes[num_bytes];
    for (size_t i = 0; i < num_bytes; i++) {
        le_bytes[i] = (num >> (8 * i)) & 0xFF;
        be_bytes[i] = (num >> (8 * (num_bytes - i - 1))) & 0xFF;
    }

    unsigned char buffer[1024];
    size_t bytesRead, totalBytes = 0;

    while ((bytesRead = fread(buffer + totalBytes, 1, sizeof(buffer) - totalBytes, file)) > 0 || totalBytes >= num_bytes) {
        totalBytes += bytesRead;

        for (size_t i = 0; i <= totalBytes - num_bytes; ++i) {
            if (memcmp(&buffer[i], le_bytes, num_bytes) == 0 || memcmp(&buffer[i], be_bytes, num_bytes) == 0) {
                if (DEBUG) {
                    fprintf(stderr, "DEBUG: Found the sequence at position %zu\n", i);
                }
                fclose(file);
                return 0;
            }
        }

        if (totalBytes > num_bytes) {
            memmove(buffer, buffer + totalBytes - num_bytes + 1, num_bytes - 1);
            totalBytes = num_bytes - 1;
        }
    }

    if (DEBUG) {
        fprintf(stderr, "DEBUG: Sequence not found\n");
    }

    fclose(file);
    return 1;
}