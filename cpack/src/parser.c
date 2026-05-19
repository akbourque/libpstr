#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"
#include "vec_cpack_dependency.h"

void cpack_dependencies_deep_free(vec_cpack_dependency_t *v) {
    if (v == NULL) return;
    
    // Explicitly loop through and tear down the heap values inside every slot
    for (size_t i = 0; i < v->len; i++) {
        if (v->data[i].package_name != NULL) {
            pstr.free(v->data[i].package_name);
        }
        if (v->data[i].local_path != NULL) {
            pstr.free(v->data[i].local_path);
        }
    }
    
    // Now that the children are safe, drop the vector frame allocation cleanly
    vec_cpack_dependency_free(v);
}

/**
 * @brief Internal helper to trim leading/trailing whitespace from a raw C-string slice buffer.
 */
static void trim_buffer(char *out, const char *in, size_t len) {
    size_t start = 0;
    while (start < len && isspace((unsigned char)in[start])) {
        start++;
    }
    
    size_t end = len;
    while (end > start && isspace((unsigned char)in[end - 1])) {
        end--;
    }
    
    size_t out_len = end - start;
    memmove(out, in + start, out_len);
    out[out_len] = '\0';
}

/**
 * @brief Internal helper to strip surrounding double quotes from a string literal.
 */
static void strip_quotes(char *str) {
    size_t len = strlen(str);
    if (len >= 2 && str[0] == '"' && str[len - 1] == '"') {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}

void cpack_parse_line(const pstr_t *line, toml_state_t *state, cpack_meta_t *meta, vec_cpack_dependency_t *deps) {
    if (line == NULL || state == NULL || meta == NULL || deps == NULL) return;
    if (line->len == 0 || line->buf[0] == '\0') return;

    // 1. Create a local workspace buffer to manipulate characters freely
    char clean_buf[512];
    trim_buffer(clean_buf, line->buf, line->len);
    size_t clean_len = strlen(clean_buf);
    
    // 2. Clear out commentary headers or zero-length blank lines
    if (clean_len == 0 || clean_buf[0] == '#') {
        return;
    }

    // 3. Section Boundary Header Tracking Lookups
    if (clean_buf[0] == '[' && clean_buf[clean_len - 1] == ']') {
        char section_name[128];
        trim_buffer(section_name, clean_buf + 1, clean_len - 2);

        if (strcmp(section_name, "package") == 0) {
            *state = TOML_STATE_PACKAGE;
        } else if (strcmp(section_name, "dependencies") == 0) {
            *state = TOML_STATE_DEPENDENCIES;
        } else {
            *state = TOML_STATE_GLOBAL;
        }
        return;
    }

    // 4. Structural Key-Value Parsing Slices
    char *eq_ptr = strchr(clean_buf, '=');
    if (eq_ptr != NULL) {
        size_t key_len = eq_ptr - clean_buf;
        char raw_key[128];
        char raw_val[256];

        trim_buffer(raw_key, clean_buf, key_len);
        trim_buffer(raw_val, eq_ptr + 1, strlen(eq_ptr + 1));

        // Processing rules based on active container block state
        if (*state == TOML_STATE_PACKAGE) {
            strip_quotes(raw_val);
            
            if (strcmp(raw_key, "name") == 0) {
                meta->name = pstr_from_cstr(raw_val);
            } else if (strcmp(raw_key, "version") == 0) {
                meta->version = pstr_from_cstr(raw_val);
            } else if (strcmp(raw_key, "edition") == 0) {
                meta->edition = pstr_from_cstr(raw_val);
            }
        } 
        else if (*state == TOML_STATE_DEPENDENCIES) {
            // UPGRADE: Parse inline tables such as `{ path = "../libpstr" }`
            char *open_brace = strchr(raw_val, '{');
            char *close_brace = strchr(raw_val, '}');
            
            if (open_brace != NULL && close_brace != NULL) {
                // Find the internal 'path' key inside the braces
                char *path_key_ptr = strstr(open_brace, "path");
                if (path_key_ptr != NULL && path_key_ptr < close_brace) {
                    char *inner_eq = strchr(path_key_ptr, '=');
                    if (inner_eq != NULL && inner_eq < close_brace) {
                        // Extract everything from the inner '=' to the closing brace
                        size_t inner_val_len = close_brace - (inner_eq + 1);
                        char parsed_path[256];
                        
                        trim_buffer(parsed_path, inner_eq + 1, inner_val_len);
                        strip_quotes(parsed_path);
                        
                        // Instantiate an isolated record allocation slot
                        cpack_dependency_t dep;
                        dep.package_name = pstr_from_cstr(raw_key);
                        dep.local_path = pstr_from_cstr(parsed_path);
                        
                        // Push the populated structure into the container vector
                        vec_cpack_dependency_push(deps, dep);
                    }
                }
            }
        }
    }
}
