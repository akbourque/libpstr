#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include "crawler.h"
#include "vec_pstr_ptr.h"

void cpack_crawl_source_tree(const char *base_dir, vec_pstr_ptr_t *out_files) {
    if (base_dir == NULL || out_files == NULL) return;

    DIR *dir = opendir(base_dir);
    if (dir == NULL) {
        return; // Return safely if a subdirectory doesn't exist or can't be read
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Guard against infinite navigation recursion loops
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Synthesize the fully qualified sub-path string using libpstr formatting
        pstr_t *sub_path = pstr_format("%s/%s", base_dir, entry->d_name);

        if (entry->d_type == DT_DIR) {
            // Dive down into the nested sub-folder recursively!
            cpack_crawl_source_tree(sub_path->buf, out_files);
            pstr.free(sub_path); // Clean up the directory path string after scanning it
        } 
        else if (entry->d_type == DT_REG) {
            // Check if the regular file carries the target '.c' source extension
            size_t len = sub_path->len;
            if (len > 2 && sub_path->buf[len - 2] == '.' && sub_path->buf[len - 1] == 'c') {
                
                // Push the pstr_t* pointer safely into our type-safe cgen vector!
                vec_pstr_ptr_push(out_files, sub_path);
                continue; // The vector takes ownership of the string; do not free it here
            }
            pstr.free(sub_path); // Clean up non-C file paths
        } else {
            pstr.free(sub_path);
        }
    }

    closedir(dir);
}
