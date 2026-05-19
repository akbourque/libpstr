#include <stdio.h>
#include <stdlib.h>
#include "vendor/libpstr.h"
#include "manifest.h"
#include "parser.h"
#include "crawler.h" // NEW: Include workspace directory crawling interface
#include "emitter.h" // NEW: Include the updated ninja emitter interface
#include "vec_cpack_dependency.h"
#include "vec_pstr_ptr.h"       // NEW: Include our string pointer container array

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("Initializing cpack manifest compiler engine...\n");

    // 1. Parse cpack.toml manifest
    FILE *f = fopen("cpack.toml", "r");
    if (f == NULL) {
        fprintf(stderr, "Error: Could not locate local cpack.toml manifest.\n");
        return 1;
    }

    toml_state_t current_state = TOML_STATE_GLOBAL;
    cpack_meta_t package_metadata = { .name = NULL, .version = NULL, .edition = NULL };
    
    vec_cpack_dependency_t dependencies;
    vec_cpack_dependency_init(&dependencies);

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), f) != NULL) {
        pstr_t *line = pstr_from_cstr(buffer);
        cpack_parse_line(line, &current_state, &package_metadata, &dependencies);
        pstr.free(line);
    }
    fclose(f);

    // 2. Discover active source files recursively across workspace trees
    vec_pstr_ptr_t source_files;
    vec_pstr_ptr_init(&source_files);
    cpack_crawl_source_tree("src", &source_files);

    // 3. Output completed diagnostic summaries to confirmation console
    printf("\n--- PARSING COMPLETED RESULT ---\n");
    if (package_metadata.name != NULL) {
        printf("Package Identity : %s\n", package_metadata.name->buf);
    }
    if (package_metadata.version != NULL) {
        printf("Semantic Version : %s\n", package_metadata.version->buf);
    }
    if (package_metadata.edition != NULL) {
        printf("Compiler Edition : %s\n", package_metadata.edition->buf);
    }
    
    printf("Total Dependencies: %zu\n", dependencies.len);
    for (size_t i = 0; i < dependencies.len; i++) {
        printf("  -> [%zu] Name: %-12s | Path: %s\n", 
               i, dependencies.data[i].package_name->buf, dependencies.data[i].local_path->buf);
    }

    // Print out dynamically crawled source tree elements
    printf("Discovered Source Files: %zu\n", source_files.len);
    for (size_t i = 0; i < source_files.len; i++) {
        printf("  -> [%zu] Path: %s\n", i, source_files.data[i]->buf);
    }
    printf("--------------------------------\n");

    // 4. Launch updated dynamic code emission backend
    printf("Emitting optimized Ninja build blueprint...\n");
    if (cpack_emit_ninja("build.ninja", &package_metadata, &dependencies, &source_files)) {
        printf("✨ Success! 'build.ninja' script generated perfectly.\n");
    } else {
        fprintf(stderr, "❌ Error: Code generation phase failed.\n");
    }

    // 5. Clean up allocations completely
    if (package_metadata.name != NULL) pstr.free(package_metadata.name);
    if (package_metadata.version != NULL) pstr.free(package_metadata.version);
    if (package_metadata.edition != NULL) pstr.free(package_metadata.edition);
    
    cpack_dependencies_deep_free(&dependencies);

    // Deep clean sub-allocated path strings before dropping vector frame
    for (size_t i = 0; i < source_files.len; i++) {
        pstr.free(source_files.data[i]);
    }
    vec_pstr_ptr_free(&source_files);

    return 0;
}
