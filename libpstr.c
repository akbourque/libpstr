#define _GNU_SOURCE
#include <stdlib.h>
#include <assert.h>
// #include <string.h>
#include <stdio.h>
#include "libpstr.h"
// Macros for optimization hints
#define unlikely(x) __builtin_expect(!!(x), 0)

// interface funtions
const pstr_functions pstr = {
    .alloc = pstr_new,
    .free = pstr_free,
    .from_cstr = pstr_from_cstr,
    .substring = pstr_substring,
    .find_cstr = pstr_find_cstr,
    .byte_to_char = pstr_byte_to_char,
    .find_pstr = pstr_find_pstr,
    .from_utf8_lossy = pstr_from_utf8_lossy,
    .transform = pstr_transform,
    .from_slice = pstr_from_slice,
    .split = pstr_split,
    .starts_with = pstr_starts_with,
    .ends_with = pstr_ends_with,
    .trim = pstr_trim,
    .slice_to_index = pstr_slice_to_index,
    .version = pstr_version, 
    .format = pstr_format,
    .format_v = pstr_format_v,
    .read_from_file = pstr_read_from_file,
    .read_from_file_lossy = pstr_read_from_file_lossy,

    .builder = {
        .init = pstr_builder_init,
        .cleanup = pstr_builder_cleanup,
        .append = pstr_builder_append,
        .append_substring = pstr_builder_append_substring,
        .append_cstr = pstr_builder_append_cstr,
        .append_pstr = pstr_builder_append_pstr,
        .appendf = pstr_builder_appendf,
        .append_utf8 = pstr_builder_append_utf8,
        .build = pstr_builder_build,
        .replace_range = pstr_builder_replace_range,
        .replace_range_cstr = pstr_builder_replace_range_cstr,
        .replace_range_pstr = pstr_builder_replace_range_pstr,
        .vappendf = pstr_builder_vappendf,
        .find_cstr = pstr_builder_find_cstr,
        .read_line = pstr_builder_read_line,
        .read_line_lossy = pstr_builder_read_line_lossy,
    },

    .utf8 = {
        .iter = {
            .init = pstr_iter_init,
            .next = pstr_iter_next,
        },
        .len = pstr_utf8_len,
        .is_char_boundary = pstr_is_char_boundary,
        .is_char_boundary_at_ptr = pstr_is_char_boundary_at_ptr,
        .validate = pstr_utf8_validate,
        .validate_cstr = pstr_utf8_validate_cstr,
        .validate_pstr = pstr_utf8_validate_pstr,
    },

    .vec = {
        .init = pstr_vec_init,
        .reserve = pstr_vec_reserve,
        .resize = pstr_vec_resize,
        .pop = pstr_vec_pop,
        .push = pstr_vec_push,
        .remove = pstr_vec_remove,
        .swap_remove = pstr_vec_swap_remove,
        .free = pstr_vec_free,
        .extend = pstr_vec_extend,
        .init_external = pstr_vec_init_external,
    },
    .list = {
        .free = pstr_list_free,
        .len = pstr_list_len,
        .get = pstr_list_get,
        .init = pstr_list_init,
        .push = pstr_list_push,
        .pop = pstr_list_pop,
    }
    
};

pstr_t* pstr_new(const char *str, size_t len) {
    if (str && !pstr_utf8_validate(str,len)) {
        return NULL;
    }
    pstr_t *s = (pstr_t*)malloc(sizeof(pstr_t) + len + 1);
    if (unlikely(!s)) return NULL;
    s->len = len;
    if (str) {
        memcpy(s->buf, str, len);
    }
    s->buf[len] = '\0';
    return s;
}

pstr_status_t pstr_builder_ensure(pstr_builder_t *sb, size_t total) {
    return pstr_vec_reserve(&sb->vec, total + 1);
}

pstr_status_t pstr_builder_appendf(pstr_builder_t *sb, const char *format, ...) {
    if (!sb || !format) return PSTR_ERR_NULL;
    va_list args, args_copy;
    va_start(args, format);

    // Use a copy for the first pass to protect the original list
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (needed < 0) { // encoding failure
        va_end(args);
        return PSTR_ERR_FORMAT;
    }

    if (needed == 0) { // Ok nothing to do
        va_end(args);
        return PSTR_OK;
    }

    if (pstr_builder_ensure(sb, sb->vec.len + (size_t)needed) != PSTR_OK) {
        va_end(args);
        return PSTR_ERR_OOM;
    }

    vsnprintf((char*)sb->vec.data + sb->vec.len, (size_t)needed + 1, format, args);
    va_end(args);
    sb->vec.len += (size_t)needed;
    sb->vec.data[sb->vec.len] = '\0';
    return PSTR_OK;
}

size_t pstr_find_pstr(const pstr_t *s, const pstr_t *needle) {
    if (!s || !needle || needle->len == 0) return PSTR_NPOS;
    if (needle->len > s->len) return PSTR_NPOS;
    char* match = memmem(s->buf, s->len, needle->buf, needle-> len);
    if (!match) return -1;

    // count chars to return logical index, not byte index
    size_t char_index = 0;
    pstr_iter_t it;
    pstr_iter_init(&it, s);
    while (it.ptr < match && pstr_iter_next(&it)) {
        ++char_index;
    }
    return char_index;
}

/**
 * @brief Searches for a C-string within an owned p-string.
 * @return A pstr_slice_t. If not found or invalid UTF-8, returns {NULL, 0}.
 */
pstr_slice_t pstr_find_cstr(const pstr_t *s, const char *needle) {
    if (!s || !needle) return (pstr_slice_t){NULL, 0};
    
    // Fix for Issue #3: Validate needle UTF-8 [cite: 4]
    size_t nlen = strlen(needle);
    if (nlen == 0 || nlen > s->len || !pstr_utf8_validate(needle, nlen)) {
        return (pstr_slice_t){NULL, 0};
    }

    char *match = memmem(s->buf, s->len, needle, nlen);
    if (!match) return (pstr_slice_t){NULL, 0};

    return (pstr_slice_t){ .ptr = match, .len = nlen }; // Return the copy [cite: 1, 2]
}


size_t pstr_byte_to_char(const pstr_t *s, size_t byte_idx) {
    if (!s || byte_idx >= s->len) return PSTR_NPOS;

    size_t char_index = 0;
    pstr_iter_t it;
    pstr_iter_init(&it, s);

    // Iterate while the next character's START is less than or equal to our target byte
    while (pstr_iter_next(&it)) {
        // it.ptr points to the NEXT char, so (it.ptr - it.byte_len) is the current char start
        if ((it.ptr - it.byte_len) > (s->buf + byte_idx)) {
            break;
        }
        if ((it.ptr - it.byte_len) <= (s->buf + byte_idx) && (it.ptr > (s->buf + byte_idx))) {
            return char_index; // Found the char containing the byte 
        }
        ++char_index;
    }
    return char_index;
}


pstr_status_t
pstr_builder_replace_range(pstr_builder_t *sb,
                      size_t start,
                      size_t len,
                      const char *replace_with,
                      size_t replace_len) {
    // 1. Bounds & UTF-8 Boundary Checks
    if (start + len > sb->vec.len) return PSTR_ERR_BOUNDS;
    
    // Safety check: Don't slice characters!
    if (!pstr_is_char_boundary_at_ptr((char*)sb->vec.data, sb->vec.len, start) ||
        !pstr_is_char_boundary_at_ptr((char*)sb->vec.data, sb->vec.len, start + len)) {
        PANIC("pstr.sb.replace_range: indices must be on UTF-8 boundaries");
    }

    if (replace_with != NULL && !pstr_utf8_validate(replace_with, replace_len)) {
        return PSTR_ERR_UTF8;
    }

    // 2. Calculate new total length
    size_t new_total = sb->vec.len - len + replace_len;
    if (pstr_builder_ensure(sb, new_total) != PSTR_OK) return PSTR_ERR_OOM;

    // 3. Move the "tail" using memmove for safety with overlapping regions
    uint8_t *tail_src = sb->vec.data + start + len;
    uint8_t *tail_dst = sb->vec.data + start + replace_len;
    size_t tail_len = sb->vec.len - (start + len);
    
    if (tail_len > 0) {
        memmove(tail_dst, tail_src, tail_len);
    }

    // 4. Copy the new string into the gap
    if (replace_len > 0 && replace_with != NULL) {
        memcpy(sb->vec.data + start, replace_with, replace_len);
    }

    sb->vec.len = new_total;
    sb->vec.data[sb->vec.len] = '\0';
    return PSTR_OK;
}

pstr_status_t pstr_transform(pstr_t **self, pstr_transform_fn transform) {
    if (!self || !*self || !transform) return PSTR_ERR_NULL;

    pstr_t *original = *self;
    pstr_builder_t sb;
    pstr_builder_init(&sb);

    // Pre-reserve to avoid multiple small reallocs if it grows
    if (pstr_vec_reserve(&sb.vec, original->len + 1) != PSTR_OK) {
        return PSTR_ERR_OOM;
    }

    pstr_iter_t it;
    pstr_iter_init(&it, original);

    // 1. STAGING PHASE: Transform everything into the builder
    while (pstr_iter_next(&it)) {
        uint32_t new_cp = transform(it.code_point);
        
        // Validate the output of the transform function
        pstr_status_t status = pstr_builder_append_utf8(&sb, new_cp);
        if (status != PSTR_OK) {
            pstr_builder_cleanup(&sb);
            return status; // Returns PSTR_ERR_UTF8 or PSTR_ERR_OOM
        }
    }

    // 2. COMMIT PHASE: Transformation was successful
    if (sb.vec.len == original->len) {
        // Optimization: Sizes match, just overwrite the original buffer
        memcpy(original->buf, sb.vec.data, original->len);
        // Ensure null termination for safety
        original->buf[original->len] = '\0';
        pstr_builder_cleanup(&sb);
    } else {
        // Sizes differ: We must replace the original pointer
        pstr_t *new_s = pstr_builder_build(&sb);
        pstr_builder_cleanup(&sb);

        if (!new_s) return PSTR_ERR_OOM;

        pstr_free(original);
        *self = new_s;
    }

    return PSTR_OK;
}



pstr_status_t pstr_split(const pstr_t *self, const char *delim, pstr_list_t *out_list) {
    if (!self || !delim || !out_list) return PSTR_ERR_NULL;
    if (!pstr_utf8_validate_cstr(delim)) return PSTR_ERR_UTF8;

    size_t delim_len = strlen(delim);
    if (delim_len == 0) return PSTR_ERR_FORMAT;

    size_t start = 0;
    const char *match;

    // Use our new init to ensure clean state
    pstr_list_init(out_list);

    while (start < self->len && 
          (match = memmem(self->buf + start, self->len - start, delim, delim_len))) {
        
        pstr_slice_t slice = {
            .ptr = self->buf + start,
            .len = (size_t)(match - self->buf) - start
        };

        // Encapsulated push
        pstr_list_push(out_list, slice);
        start = (size_t)(match - self->buf) + delim_len;
    }

    pstr_slice_t final = { .ptr = self->buf + start, .len = self->len - start };
    pstr_list_push(out_list, final);

    return PSTR_OK;
}


// Helper to check for whitespace (including common Unicode ones)
static inline bool is_pstr_whitespace(uint32_t cp) {
    // 0x09 to 0x0D covers \t, \n, \v, \f, \r
    // 0x20 is ' ' (Space)
    // 0x00A0 is non-breaking space
    return (cp == 0x20) || (cp >= 0x09 && cp <= 0x0D) || (cp == 0xA0);
}

pstr_slice_t pstr_trim(const pstr_t *self) {
    if (!self || self->len == 0) {
        return (pstr_slice_t){ .ptr = NULL, .len = 0 };
    }

    pstr_iter_t it;
    pstr_iter_init(&it, self);
    
    const char *start_ptr = NULL;
    const char *end_ptr = NULL;

    // 1. Find the first non-whitespace character
    while (pstr_iter_next(&it)) {
        if (!is_pstr_whitespace(it.code_point)) {
            start_ptr = it.ptr - it.byte_len;
            end_ptr = it.ptr; // In case there's only one char
            break;
        }
    }

    // If we never found a non-whitespace char, the string is all whitespace
    if (!start_ptr) {
        return (pstr_slice_t){ .ptr = self->buf, .len = 0 };
    }

    // 2. Continue to find the last non-whitespace character
    while (pstr_iter_next(&it)) {
        if (!is_pstr_whitespace(it.code_point)) {
            end_ptr = it.ptr;
        }
    }

    return (pstr_slice_t){ 
        .ptr = start_ptr, 
        .len = (size_t)(end_ptr - start_ptr) 
    };
}


bool pstr_starts_with(const pstr_t *self, const char *prefix) {
    if (!self || !prefix) return false;
    size_t plen = strlen(prefix);
    if (plen > self->len) return false;
    return memcmp(self->buf, prefix, plen) == 0;
}

bool pstr_ends_with(const pstr_t *self, const char *suffix) {
    if (!self || !suffix) return false;
    size_t slen = strlen(suffix);
    if (slen > self->len) return false;
    return memcmp(self->buf + (self->len - slen), suffix, slen) == 0;
}


pstr_slice_t pstr_list_get(const pstr_list_t* list, size_t index) {
    if (!list || list->vec.len <= PSTR_SLICE_SIZE * index) return (pstr_slice_t) {NULL, 0};
    pstr_slice_t slice;
    memcpy(&slice, list->vec.data + PSTR_SLICE_SIZE * index, PSTR_SLICE_SIZE);
    return slice;
}

pstr_status_t pstr_builder_vappendf(pstr_builder_t *sb, const char *format, va_list args) {
    if (!sb || !format) return PSTR_ERR_NULL; // Fix for Issue #10

    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (needed < 0) return PSTR_ERR_FORMAT;
    if (needed == 0) return PSTR_OK;

    if (pstr_builder_ensure(sb, sb->vec.len + (size_t)needed) != PSTR_OK) {
        return PSTR_ERR_OOM;
    }

    vsnprintf((char*)sb->vec.data + sb->vec.len, (size_t)needed + 1, format, args);
    sb->vec.len += (size_t)needed;
    sb->vec.data[sb->vec.len] = '\0'; // Guarantee null termination [cite: 16]
    return PSTR_OK;
}

/**
 * @brief The core formatting logic using va_list.
 */
pstr_t* pstr_format_v(const char* format, va_list args) {
    if (!format) return NULL; // Fix for Issue #10

    pstr_builder_t sb;
    pstr_builder_init(&sb);
    
    // We assume pstr_builder_vappendf is our factored-out helper
    pstr_status_t status = pstr_builder_vappendf(&sb, format, args);
    
    pstr_t* result = (status == PSTR_OK) ? pstr_builder_build(&sb) : NULL;
    pstr_builder_cleanup(&sb);
    return result;
}

/**
 * @brief The variadic convenience wrapper.
 */
pstr_t* pstr_format(const char* format, ...) {
    va_list args;
    va_start(args, format);
    pstr_t* result = pstr_format_v(format, args);
    va_end(args);
    return result;
}

static pstr_status_t pstr_builder_read_line_raw(pstr_builder_t *sb, FILE *fp) {
    if (!sb || !fp) return PSTR_ERR_NULL;
    sb->vec.len = 0; // Clear length, maintain capacity

    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (pstr_vec_push(&sb->vec, (uint8_t)c) != PSTR_OK) return PSTR_ERR_OOM;
        if (c == '\n') break;
    }

    if (sb->vec.len == 0 && c == EOF) return PSTR_ERR_BOUNDS; // End of File signal
    
    // Null-terminate so builder.find_cstr works [cite: 16]
    if (pstr_builder_ensure(sb, sb->vec.len) == PSTR_OK) {
        sb->vec.data[sb->vec.len] = '\0';
    }
    return PSTR_OK;
}

pstr_status_t pstr_builder_read_line(pstr_builder_t *sb, FILE *fp) {
    pstr_status_t status = pstr_builder_read_line_raw(sb, fp);
    if (status != PSTR_OK) return status;

    // Strict Validation
    if (!pstr_utf8_validate((char*)sb->vec.data, sb->vec.len)) {
        sb->vec.len = 0; // Discard invalid data
        return PSTR_ERR_UTF8;
    }

    return PSTR_OK;
}

pstr_status_t pstr_builder_read_line_lossy(pstr_builder_t *sb, FILE *fp) {
    pstr_status_t status = pstr_builder_read_line_raw(sb, fp);
    if (status != PSTR_OK) return status;

    if (!pstr_utf8_validate((char*)sb->vec.data, sb->vec.len)) {
        // Repair the line in-place using our existing lossy logic
        pstr_t* fixed = pstr_from_utf8_lossy((char*)sb->vec.data, sb->vec.len);
        if (!fixed) return PSTR_ERR_OOM;

        // Efficiently replace the builder contents
        sb->vec.len = 0;
        pstr_builder_append(sb, fixed->buf, fixed->len);
        pstr_free(fixed);
    }

    return PSTR_OK;
}


pstr_t* pstr_read_from_file(const char* path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    pstr_builder_t sb;
    pstr_builder_init(&sb);
    if (pstr_vec_reserve(&sb.vec, (size_t)fsize + 1) != PSTR_OK) {
        fclose(f);
        return NULL;
    }

    fread(sb.vec.data, 1, fsize, f);
    sb.vec.len = (size_t)fsize;
    fclose(f);

    // Validation: Strict version
    if (!pstr_utf8_validate((char*)sb.vec.data, sb.vec.len)) {
        pstr_builder_cleanup(&sb);
        return NULL;
    }

    pstr_t* result = pstr_builder_build(&sb);
    pstr_builder_cleanup(&sb);
    return result;
}

/**
 * @brief Reads an entire file into a new p-string, repairing invalid UTF-8.
 * @param path The filesystem path to the file.
 * @return A new pstr_t pointer, or NULL on OOM or file-not-found.
 */
pstr_t* pstr_read_from_file_lossy(const char* path) {
    if (!path) return NULL;

    FILE *f = fopen(path, "rb"); // Read in binary mode
    if (!f) return NULL;

    // 1. Determine file size to minimize reallocations
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    // 2. Use a builder as our workspace
    pstr_builder_t sb;
    pstr_builder_init(&sb);
    
    // Pre-allocate based on file size + 1 for null terminator
    if (pstr_vec_reserve(&sb.vec, (size_t)fsize + 1) != PSTR_OK) {
        fclose(f);
        return NULL;
    }

    // 3. Slurp the bytes
    size_t bytes_read = fread(sb.vec.data, 1, (size_t)fsize, f);
    sb.vec.len = bytes_read;
    fclose(f);

    // 4. Validate and Repair if necessary
    if (!pstr_utf8_validate((char*)sb.vec.data, sb.vec.len)) {
        // Use our existing lossy repair logic
        pstr_t* fixed = pstr_from_utf8_lossy((char*)sb.vec.data, sb.vec.len);
        pstr_builder_cleanup(&sb); // Done with the raw builder
        return fixed; // Note: fixed is already a pstr_t*
    }

    // 5. If it was already valid, just build it
    pstr_t* result = pstr_builder_build(&sb);
    pstr_builder_cleanup(&sb);
    return result;
}


/**
 * @brief Searches for a cstr in a builder's buffer.
 * * @code
 *    pstr_builder_t sb;
 *    pstr.builder.init(&sb);
 *
 *   while (pstr.builder.read_line(&sb, input_file) == PSTR_OK) {
 *        pstr_slice_t match = pstr.builder.find_cstr(&sb, "OLD_VALUE");
 *        if (match.ptr) {
 *            size_t idx = (size_t)(match.ptr - (char*)sb.vec.data);
 *            pstr.builder.replace_range(&sb, idx, match.len, "NEW_VALUE", 9);
 *       }
 *       fwrite(sb.vec.data, 1, sb.vec.len, output_file);
 *   }
 *   pstr.builder.cleanup(&sb);
 * @endcode
 * @param sb The builder.
 * @param needle The C string to find.
 * @return Returns a pstr_slice_t with pstr_slice_t.ptr == NULL if the string
 * is not found, invalid arguements, and for invalid UTF-8 in needle.
 */
pstr_slice_t pstr_builder_find_cstr(pstr_builder_t *sb, const char *needle) {
    if (!sb || !needle || !pstr_utf8_validate_cstr(needle)) return (pstr_slice_t){NULL, 0};
    
    char *match = (char*)memmem(sb->vec.data, sb->vec.len, needle, strlen(needle));
    if (!match) return (pstr_slice_t){NULL, 0};
    
    return (pstr_slice_t){ .ptr = match, .len = strlen(needle) };
}
