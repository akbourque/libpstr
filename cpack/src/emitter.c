#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "emitter.h"

bool cpack_emit_ninja(const char *manifest_path, const cpack_meta_t *meta, 
                      const vec_cpack_dependency_t *deps, const vec_pstr_ptr_t *source_files) {
    if (manifest_path == NULL || meta == NULL || deps == NULL || source_files == NULL) return false;
    if (meta->name == NULL) return false;

    FILE *f = fopen(manifest_path, "w");
    if (f == NULL) {
        fprintf(stderr, "Error: Failed to open destination path '%s' for writing.\n", manifest_path);
        return false;
    }

    // 1. Emit Header & Global Configurations
    fprintf(f, "# Generated automatically by cpack v0.1.0\n");
    fprintf(f, "# Project: %s\n\n", meta->name->buf);
    
    fprintf(f, "cc = gcc\n");

    char edition_flag[32] = "c11"; // Fallback default
    if (meta->edition != NULL && meta->edition->len < sizeof(edition_flag)) {
        for (size_t i = 0; i < meta->edition->len; i++) {
            edition_flag[i] = (char)tolower((unsigned char)meta->edition->buf[i]);
        }
        edition_flag[meta->edition->len] = '\0';
    }
    fprintf(f, "cflags = -Wall -Wextra -std=%s -g -Isrc -Isrc/vendor\n\n", edition_flag);

    // 2. Emit Reusable Toolchain Blueprint Rules
    fprintf(f, "# Reusable compilation toolchain rules\n");
    fprintf(f, "rule cc\n");
    fprintf(f, "  command = $cc $cflags -c $in -o $out\n");
    fprintf(f, "  description = Compiling C Object: $out\n\n");

    fprintf(f, "rule link\n");
    fprintf(f, "  command = $cc $in -o $out\n");
    fprintf(f, "  description = Linking Binary: $out\n\n");

    // 3. Emit Transitive Dependency Build Rules
    if (deps->len > 0) {
        fprintf(f, "# Sub-project dependency compilation blocks\n");
        for (size_t i = 0; i < deps->len; i++) {
            fprintf(f, "# Dependency: %s (Location: %s)\n", 
                    deps->data[i].package_name->buf, 
                    deps->data[i].local_path->buf);
        }
        fprintf(f, "\n");
    }

    // 4. Emit Local Application Build Edges Dynamically!
    fprintf(f, "# Application target explicit build edges\n");
    
    // Loop over every crawled file to print its individual compilation object edge line
    for (size_t i = 0; i < source_files->len; i++) {
        const char *src_path = source_files->data[i]->buf;
        
        // Synthesize an object target filename by copying and changing '.c' into '.o'
        char obj_path[512];
        strncpy(obj_path, src_path, sizeof(obj_path) - 1);
        obj_path[sizeof(obj_path) - 1] = '\0';
        
        size_t path_len = strlen(obj_path);
        if (path_len > 2 && obj_path[path_len - 2] == '.' && obj_path[path_len - 1] == 'c') {
            obj_path[path_len - 1] = 'o'; // Transform trailing character 'c' -> 'o'
        }
        
        fprintf(f, "build %s: cc %s\n", obj_path, src_path);
    }
    fprintf(f, "\n");

    // 5. Emit Master Linking Edge targeting all generated objects simultaneously
    fprintf(f, "build %s: link", meta->name->buf);
    for (size_t i = 0; i < source_files->len; i++) {
        const char *src_path = source_files->data[i]->buf;
        
        char obj_path[512];
        strncpy(obj_path, src_path, sizeof(obj_path) - 1);
        obj_path[sizeof(obj_path) - 1] = '\0';
        
        size_t path_len = strlen(obj_path);
        if (path_len > 2 && obj_path[path_len - 2] == '.' && obj_path[path_len - 1] == 'c') {
            obj_path[path_len - 1] = 'o';
        }
        
        fprintf(f, " %s", obj_path);
    }
    fprintf(f, "\n\n");

    // 6. Establish Default Target Entrypoint
    fprintf(f, "default %s\n", meta->name->buf);

    fclose(f);
    return true;
}
