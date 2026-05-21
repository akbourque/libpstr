#define _GNU_SOURCE
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include "libpstr.h"

#define unlikely(x) __builtin_expect(!!(x), 0)

static libpstr_slice_t slice_trim(libpstr_slice_t slice);
static bool slice_split_once(libpstr_slice_t src, const char *delim, libpstr_slice_t *left, libpstr_slice_t *right);
static bool slice_starts_with(libpstr_slice_t slice, const char *prefix);
static bool  slice_ends_with(libpstr_slice_t slice, const char *suffix);

/* =========================================================================
 * 📦 SUB-NAMESPACE SECTION 1: RAW BYTE VECTORS (libpstr.vec)
 * ========================================================================= */

static void vec_init(libpstr_vec_t *v, size_t initial_cap) {
    v->len = 0;
    v->cap = initial_cap;
    if (initial_cap > 0) {
        v->data = (uint8_t*)malloc(initial_cap);
        if (!v->data) {
            PANIC("libpstr.vec.init: Out of memory during initial allocation");
        }
        v->is_heap = true;
    } else {
        v->data = NULL;
        v->is_heap = false;
    }
}

static void vec_init_external(libpstr_vec_t *v, uint8_t *buf, size_t cap) {
    v->len = 0;
    v->cap = cap;
    v->is_heap = false;
    v->data = buf;
}

static void vec_free(libpstr_vec_t *v) {
    if (v && v->data && v->is_heap) {
        free(v->data);
    }
    if (v) {
        v->data = NULL;
        v->len = 0;
        v->cap = 0;
        v->is_heap = false;
    }
}

static libpstr_status_t vec_reserve(libpstr_vec_t *v, size_t needed) {
    if (v->cap >= needed) return LIBPSTR_OK;

    size_t ncap = (v->cap == 0) ? 128 : v->cap * 2;
    while (ncap < needed) ncap *= 2;

    uint8_t *ndata;
    if (v->is_heap) {
        ndata = (uint8_t*)realloc(v->data, ncap);
    } else {
        ndata = (uint8_t*)malloc(ncap);
        if (ndata && v->len > 0) {
            memcpy(ndata, v->data, v->len);
        }
        v->is_heap = true;
    }

    if (!ndata) return LIBPSTR_ERR_OOM;
    v->data = ndata;
    v->cap = ncap;
    return LIBPSTR_OK;
}

static libpstr_status_t vec_push(libpstr_vec_t *v, uint8_t byte) {
    if (vec_reserve(v, v->len + 1) != LIBPSTR_OK) return LIBPSTR_ERR_OOM;
    v->data[v->len++] = byte;
    return LIBPSTR_OK;
}

static libpstr_status_t vec_append(libpstr_vec_t *v, const void *src, size_t len) {
    if (!src || len == 0) return LIBPSTR_OK;
    if (vec_reserve(v, v->len + len) != LIBPSTR_OK) return LIBPSTR_ERR_OOM;
    memcpy(v->data + v->len, src, len);
    v->len += len;
    return LIBPSTR_OK;
}

static libpstr_status_t vec_extend(libpstr_vec_t *v, const uint8_t *bytes, size_t count) {
    return vec_append(v, bytes, count);
}

static libpstr_status_t vec_remove(libpstr_vec_t *v, size_t index) {
    if (index >= v->len) return LIBPSTR_ERR_BOUNDS;
    memmove(v->data + index, v->data + index + 1, v->len - index - 1);
    v->len--;
    return LIBPSTR_OK;
}

static libpstr_status_t vec_swap_remove(libpstr_vec_t *v, size_t index) {
    if (index >= v->len) return LIBPSTR_ERR_BOUNDS;
    v->data[index] = v->data[v->len - 1];
    v->len--;
    return LIBPSTR_OK;
}

static void vec_pop(libpstr_vec_t *v) {
    if (v->len > 0) v->len--;
    else PANIC("libpstr.vec.pop: attempt to pop from an empty vector");
}

static libpstr_status_t vec_resize(libpstr_vec_t *v, size_t new_len) {
    if (new_len <= v->len) {
        v->len = new_len;
        return LIBPSTR_OK;
    }
    if (vec_reserve(v, new_len) != LIBPSTR_OK) return LIBPSTR_ERR_OOM;
    memset(v->data + v->len, 0, new_len - v->len);
    v->len = new_len;
    return LIBPSTR_OK;
}

/* =========================================================================
 * 📦 SUB-NAMESPACE SECTION 2: UTF-8 TEXT VALIDATION ENGINE (libpstr.utf8)
 * ========================================================================= */

static bool utf8_validate(const char *str, size_t len) {
    if (!str) return false;
    const uint8_t *p = (const uint8_t *)str;
    const uint8_t *end = p + len;

    while (p < end) {
        if (*p <= 0x7F) {
            p++;
        } else if ((*p & 0xE0) == 0xC0) {
            if (p + 1 >= end || (p[1] & 0xC0) != 0x80) return false;
            if ((*p & 0x1F) < 0x02) return false;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            if (p + 2 >= end || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return false;
            uint32_t cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            if (cp < 0x0800 || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
            p += 3;
        } else if ((*p & 0xF8) == 0xF0) {
            if (p + 3 >= end || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80) return false;
            uint32_t cp = ((*p & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
            if (cp < 0x10000 || cp > 0x10FFFF) return false;
            p += 4;
        } else {
            return false;
        }
    }
    return true;
}

static bool utf8_validate_cstr(const char *str) {
    return str ? utf8_validate(str, strlen(str)) : false;
}

static bool utf8_validate_pstr(const libpstr_pstr_t *str) {
    return str ? utf8_validate(str->buf, str->len) : false;
}

static size_t utf8_len(const libpstr_pstr_t *s) {
    if (!s || s->len == 0) return 0;
    size_t count = 0;
    for (size_t i = 0; i < s->len; i++) {
        if (((unsigned char)s->buf[i] & 0xC0) != 0x80) {
            count++;
        }
    }
    return count;
}

static bool is_char_boundary_at_ptr(const char *buf, size_t len, size_t index) {
    if (index == 0 || index == len) return true;
    if (index > len) return false;
    return ((unsigned char)buf[index] & 0xC0) != 0x80;
}

static bool is_char_boundary(const libpstr_pstr_t *s, size_t byte_idx) {
    if (!s) return false;
    return is_char_boundary_at_ptr(s->buf, s->len, byte_idx);
}

static void iter_init(libpstr_iter_t *it, const libpstr_pstr_t *s) {
    if (!s) {
        it->ptr = it->end = NULL;
        return;
    }
    it->ptr = s->buf;
    it->end = s->buf + s->len;
    it->code_point = 0;
    it->byte_len = 0;
}

static bool iter_next(libpstr_iter_t *it) {
    if (it->ptr >= it->end) return false;

    unsigned char c = (unsigned char)*it->ptr;
    uint32_t cp = 0;
    int len = 0;

    if (c <= 0x7F) { cp = c; len = 1; }
    else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
    else return false;

    if (it->ptr + len > it->end) return false;

    for (int i = 1; i < len; i++) {
        unsigned char next_c = (unsigned char)it->ptr[i];
        if ((next_c & 0xC0) != 0x80) return false;
        cp = (cp << 6) | (next_c & 0x3F);
    }

    it->code_point = cp;
    it->byte_len = len;
    it->ptr += len;
    return true;
}

/* Helper character decoder designed for raw slice windows */
static bool slice_decode_step(const char **ptr, const char *end, uint32_t *cp, int *bytes) {
    if (*ptr >= end) return false;
    unsigned char c = (unsigned char)**ptr;
    uint32_t code = 0;
    int len = 0;

    if (c <= 0x7F) { code = c; len = 1; }
    else if ((c & 0xE0) == 0xC0) { code = c & 0x1F; len = 2; }
    else if ((c & 0xF0) == 0xE0) { code = c & 0x0F; len = 3; }
    else if ((c & 0xF8) == 0xF0) { code = c & 0x07; len = 4; }
    else return false;

    if (*ptr + len > end) return false;

    for (int i = 1; i < len; i++) {
        unsigned char next_c = (unsigned char)(*ptr)[i];
        if ((next_c & 0xC0) != 0x80) return false;
        code = (code << 6) | (next_c & 0x3F);
    }
    *cp = code;
    *bytes = len;
    return true;
}

static inline bool is_pstr_whitespace(uint32_t cp) {
    return (cp == 0x20) || (cp >= 0x09 && cp <= 0x0D) || (cp == 0xA0);
}

/* =========================================================================
 * 📦 SUB-NAMESPACE SECTION 3: REUSABLE STACK LISTS (libpstr.list)
 * ========================================================================= */

static void list_init(libpstr_list_t *list) {
    if (list) vec_init(&list->vec, 0);
}

static void list_free(libpstr_list_t *list) {
    if (list) vec_free(&list->vec);
}

static size_t list_len(const libpstr_list_t *list) {
    return list ? list->vec.len / LIBPSTR_SLICE_SIZE : 0;
}

static libpstr_slice_t list_get(const libpstr_list_t *list, size_t index) {
    if (!list || list->vec.len <= LIBPSTR_SLICE_SIZE * index) {
        return (libpstr_slice_t){NULL, 0};
    }
    libpstr_slice_t slice;
    memcpy(&slice, list->vec.data + LIBPSTR_SLICE_SIZE * index, LIBPSTR_SLICE_SIZE);
    return slice;
}

static libpstr_status_t list_push(libpstr_list_t *list, libpstr_slice_t slice) {
    if (!list) return LIBPSTR_ERR_NULL;
    return vec_append(&list->vec, &slice, LIBPSTR_SLICE_SIZE);
}

static libpstr_slice_t list_pop(libpstr_list_t *list) {
    if (!list || list->vec.len < LIBPSTR_SLICE_SIZE) {
        PANIC("libpstr.list.pop: attempt to pop from an empty slice list boundary");
    }
    libpstr_slice_t slice = list_get(list, list_len(list) - 1);
    list->vec.len -= LIBPSTR_SLICE_SIZE;
    return slice;
}

/* =========================================================================
 * 📦 SUB-NAMESPACE SECTION 4: TEXT CONSOLIDATION SPACES (libpstr.builder)
 * ========================================================================= */

static void builder_init(libpstr_builder_t *sb) {
    vec_init_external(&sb->vec, (uint8_t*)sb->sbo, LIBPSTR_BUILDER_SBO);
    sb->sbo[0] = '\0';
}

static void builder_cleanup(libpstr_builder_t *sb) {
    if (sb && sb->vec.is_heap) {
        free(sb->vec.data);
        builder_init(sb);
    }
}

static libpstr_status_t builder_ensure(libpstr_builder_t *sb, size_t total) {
    return vec_reserve(&sb->vec, total + 1);
}

static libpstr_pstr_t* builder_build(libpstr_builder_t *sb) {
    if (!sb) return NULL;
    // Uses structural allocation boundary to yield new owned layout
    libpstr_pstr_t *s = (libpstr_pstr_t*)malloc(sizeof(libpstr_pstr_t) + sb->vec.len + 1);
    if (unlikely(!s)) return NULL;
    s->len = sb->vec.len;
    if (sb->vec.len > 0) {
        memcpy(s->buf, sb->vec.data, sb->vec.len);
    }
    s->buf[s->len] = '\0';
    return s;
}

static libpstr_status_t builder_append(libpstr_builder_t *sb, const char *str, size_t len) {
    if (!str) return LIBPSTR_ERR_NULL;
    if (!utf8_validate(str, len)) return LIBPSTR_ERR_UTF8;
    if (vec_append(&sb->vec, str, len) != LIBPSTR_OK) return LIBPSTR_ERR_OOM;
    sb->vec.data[sb->vec.len] = '\0';
    return LIBPSTR_OK;
}

static libpstr_status_t builder_append_cstr(libpstr_builder_t *b, const char *str) {
    if (!str) return LIBPSTR_ERR_NULL;
    return builder_append(b, str, strlen(str));
}

static libpstr_status_t builder_append_pstr(libpstr_builder_t *b, libpstr_pstr_t *str) {
    if (!str) return LIBPSTR_ERR_NULL;
    return builder_append(b, str->buf, str->len);
}

static libpstr_status_t builder_vappendf(libpstr_builder_t *sb, const char *format, va_list args) {
    if (!sb || !format) return LIBPSTR_ERR_NULL;
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (needed < 0) return LIBPSTR_ERR_FORMAT;
    if (needed == 0) return LIBPSTR_OK;

    if (builder_ensure(sb, sb->vec.len + (size_t)needed) != LIBPSTR_OK) {
        return LIBPSTR_ERR_OOM;
    }

    vsnprintf((char*)sb->vec.data + sb->vec.len, (size_t)needed + 1, format, args);
    sb->vec.len += (size_t)needed;
    sb->vec.data[sb->vec.len] = '\0';
    return LIBPSTR_OK;
}

static libpstr_status_t builder_appendf(libpstr_builder_t *b, const char *format, ...) {
    va_list args;
    va_start(args, format);
    libpstr_status_t status = builder_vappendf(b, format, args);
    va_end(args);
    return status;
}

static libpstr_status_t builder_append_utf8(libpstr_builder_t *sb, uint32_t cp) {
    int bytes;
    if (cp <= 0x7F) bytes = 1;
    else if (cp <= 0x7FF) bytes = 2;
    else if (cp <= 0xFFFF) bytes = 3;
    else if (cp <= 0x10FFFF) bytes = 4;
    else return LIBPSTR_ERR_UTF8;

    if (builder_ensure(sb, sb->vec.len + bytes) != LIBPSTR_OK) return LIBPSTR_ERR_OOM;

    char *p = (char*)sb->vec.data + sb->vec.len;
    if (bytes == 1) {
        p[0] = (char)cp;
    } else if (bytes == 2) {
        p[0] = (char)(0xC0 | (cp >> 6));
        p[1] = (char)(0x80 | (cp & 0x3F));
    } else if (bytes == 3) {
        p[0] = (char)(0xE0 | (cp >> 12));
        p[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        p[2] = (char)(0x80 | (cp & 0x3F));
    } else {
        p[0] = (char)(0xF0 | (cp >> 18));
        p[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        p[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        p[3] = (char)(0x80 | (cp & 0x3F));
    }

    sb->vec.len += bytes;
    sb->vec.data[sb->vec.len] = '\0';
    return LIBPSTR_OK;
}

static bool builder_append_substring(libpstr_builder_t *sb, const libpstr_pstr_t *s, size_t start, size_t len) {
    if (!s) return true;
    if (start + len > s->len) return false;
    assert(is_char_boundary(s, start));
    assert(is_char_boundary(s, start + len));
    return builder_append(sb, s->buf + start, len) == LIBPSTR_OK;
}

static libpstr_status_t builder_replace_range(libpstr_builder_t *sb, size_t start, size_t len, const char *replace_with, size_t replace_len) {
    if (start + len > sb->vec.len) return LIBPSTR_ERR_BOUNDS;
    if (!is_char_boundary_at_ptr((char*)sb->vec.data, sb->vec.len, start) ||
        !is_char_boundary_at_ptr((char*)sb->vec.data, sb->vec.len, start + len)) {
        PANIC("libpstr.builder.replace_range: execution aborted, replacement coordinates split UTF-8 boundaries");
    }
    if (replace_with != NULL && !utf8_validate(replace_with, replace_len)) {
        return LIBPSTR_ERR_UTF8;
    }

    size_t new_total = sb->vec.len - len + replace_len;
    if (builder_ensure(sb, new_total) != LIBPSTR_OK) return LIBPSTR_ERR_OOM;

    uint8_t *tail_src = sb->vec.data + start + len;
    uint8_t *tail_dst = sb->vec.data + start + replace_len;
    size_t tail_len = sb->vec.len - (start + len);

    if (tail_len > 0) {
        memmove(tail_dst, tail_src, tail_len);
    }
    if (replace_len > 0 && replace_with != NULL) {
        memcpy(sb->vec.data + start, replace_with, replace_len);
    }

    sb->vec.len = new_total;
    sb->vec.data[sb->vec.len] = '\0';
    return LIBPSTR_OK;
}

static libpstr_status_t builder_replace_range_cstr(libpstr_builder_t *sb, size_t start, size_t len, const char *str) {
    size_t rlen = str ? strlen(str) : 0;
    return builder_replace_range(sb, start, len, str, rlen);
}

static libpstr_status_t builder_replace_range_pstr(libpstr_builder_t *sb, size_t start, size_t len, const libpstr_pstr_t *pstr) {
    if (!pstr) return builder_replace_range(sb, start, len, NULL, 0);
    return builder_replace_range(sb, start, len, pstr->buf, pstr->len);
}

static libpstr_slice_t builder_find_cstr(libpstr_builder_t *sb, const char *needle) {
    if (!sb || !needle || !utf8_validate_cstr(needle)) return (libpstr_slice_t){NULL, 0};
    size_t nlen = strlen(needle);
    char *match = (char*)memmem(sb->vec.data, sb->vec.len, needle, nlen);
    if (!match) return (libpstr_slice_t){NULL, 0};
    return (libpstr_slice_t){ .ptr = match, .len = nlen };
}

static libpstr_status_t builder_read_line_raw(libpstr_builder_t *sb, FILE *fp) {
    if (!sb || !fp) return LIBPSTR_ERR_NULL;
    sb->vec.len = 0;
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (vec_push(&sb->vec, (uint8_t)c) != LIBPSTR_OK) return LIBPSTR_ERR_OOM;
        if (c == '\n') break;
    }
    if (sb->vec.len == 0 && c == EOF) return LIBPSTR_ERR_BOUNDS;
    if (builder_ensure(sb, sb->vec.len) == LIBPSTR_OK) {
        sb->vec.data[sb->vec.len] = '\0';
    }
    return LIBPSTR_OK;
}

static libpstr_status_t builder_read_line(libpstr_builder_t *sb, FILE *fp) {
    libpstr_status_t status = builder_read_line_raw(sb, fp);
    if (status != LIBPSTR_OK) return status;
    if (!utf8_validate((char*)sb->vec.data, sb->vec.len)) {
        sb->vec.len = 0;
        return LIBPSTR_ERR_UTF8;
    }
    return LIBPSTR_OK;
}

/* Forward declaration to preserve pipeline flow boundaries */
static libpstr_pstr_t* pstr_from_utf8_lossy(const char *str, size_t len);

static libpstr_status_t builder_read_line_lossy(libpstr_builder_t *sb, FILE *fp) {
    libpstr_status_t status = builder_read_line_raw(sb, fp);
    if (status != LIBPSTR_OK) return status;

    if (!utf8_validate((char*)sb->vec.data, sb->vec.len)) {
        libpstr_pstr_t* fixed = pstr_from_utf8_lossy((char*)sb->vec.data, sb->vec.len);
        if (!fixed) return LIBPSTR_ERR_OOM;
        sb->vec.len = 0;
        builder_append(sb, fixed->buf, fixed->len);
        free(fixed);
    }
    return LIBPSTR_OK;
}

/* =========================================================================
 * 📦 SUB-NAMESPACE SECTION 5: OWNED HEAP OBJECT ACTIONS (libpstr.pstr)
 * ========================================================================= */

static libpstr_pstr_t* pstr_alloc(const char* str, size_t len) {
    if (str && !utf8_validate(str, len)) return NULL;
    libpstr_pstr_t *s = (libpstr_pstr_t*)malloc(sizeof(libpstr_pstr_t) + len + 1);
    if (unlikely(!s)) return NULL;
    s->len = len;
    if (str) {
        memcpy(s->buf, str, len);
    }
    s->buf[len] = '\0';
    return s;
}

static void pstr_free_str(libpstr_pstr_t *s) {
    free(s);
}

static libpstr_pstr_t* pstr_from_cstr(const char* str) {
    if (!str) return NULL;
    return pstr_alloc(str, strlen(str));
}

static libpstr_pstr_t* pstr_from_slice(libpstr_slice_t slice) {
    if (!slice.ptr) return NULL;
    return pstr_alloc(slice.ptr, slice.len);
}

static libpstr_pstr_t* pstr_from_utf8_lossy(const char *str, size_t len) {
    libpstr_builder_t sb;
    builder_init(&sb);
    const uint8_t *p = (const uint8_t *)str;
    const uint8_t *end = p + len;

    while (p < end) {
        if (*p <= 0x7F) {
            vec_push(&sb.vec, *p);
            p++;
        } else {
            int width = 0;
            if ((*p & 0xE0) == 0xC0) width = 2;
            else if ((*p & 0xF0) == 0xE0) width = 3;
            else if ((*p & 0xF8) == 0xF0) width = 4;

            bool valid = (width > 0 && (p + width <= end));
            if (valid) {
                for (int i = 1; i < width; i++) {
                    if ((p[i] & 0xC0) != 0x80) { valid = false; break; }
                }
            }
            if (valid) {
                vec_append(&sb.vec, p, width);
                p += width;
            } else {
                builder_append_utf8(&sb, 0xFFFD);
                p++;
            }
        }
    }
    libpstr_pstr_t *result = builder_build(&sb);
    builder_cleanup(&sb);
    return result;
}

static libpstr_pstr_t* pstr_format_v(const char* format, va_list args) {
    if (!format) return NULL;
    libpstr_builder_t sb;
    builder_init(&sb);
    libpstr_status_t status = builder_vappendf(&sb, format, args);
    libpstr_pstr_t* result = (status == LIBPSTR_OK) ? builder_build(&sb) : NULL;
    builder_cleanup(&sb);
    return result;
}

static libpstr_pstr_t* pstr_format(const char* format, ...) {
    va_list args;
    va_start(args, format);
    libpstr_pstr_t* result = pstr_format_v(format, args);
    va_end(args);
    return result;
}

static libpstr_pstr_t* pstr_read_from_file(const char* path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    libpstr_builder_t sb;
    builder_init(&sb);
    if (vec_reserve(&sb.vec, (size_t)fsize + 1) != LIBPSTR_OK) {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(sb.vec.data, 1, fsize, f);
    sb.vec.len = read_bytes;
    fclose(f);

    if (!utf8_validate((char*)sb.vec.data, sb.vec.len)) {
        builder_cleanup(&sb);
        return NULL;
    }
    libpstr_pstr_t* result = builder_build(&sb);
    builder_cleanup(&sb);
    return result;
}

static libpstr_pstr_t* pstr_read_from_file_lossy(const char* path) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    libpstr_builder_t sb;
    builder_init(&sb);
    if (vec_reserve(&sb.vec, (size_t)fsize + 1) != LIBPSTR_OK) {
        fclose(f);
        return NULL;
    }

    size_t bytes_read = fread(sb.vec.data, 1, (size_t)fsize, f);
    sb.vec.len = bytes_read;
    fclose(f);

    if (!utf8_validate((char*)sb.vec.data, sb.vec.len)) {
        libpstr_pstr_t* fixed = pstr_from_utf8_lossy((char*)sb.vec.data, sb.vec.len);
        builder_cleanup(&sb);
        return fixed;
    }
    libpstr_pstr_t* result = builder_build(&sb);
    builder_cleanup(&sb);
    return result;
}

static libpstr_status_t pstr_transform(libpstr_pstr_t **self, libpstr_transform_fn transform) {
    if (!self || !*self || !transform) return LIBPSTR_ERR_NULL;
    libpstr_pstr_t *original = *self;
    libpstr_builder_t sb;
    builder_init(&sb);

    if (vec_reserve(&sb.vec, original->len + 1) != LIBPSTR_OK) return LIBPSTR_ERR_OOM;

    libpstr_iter_t it;
    iter_init(&it, original);
    while (iter_next(&it)) {
        uint32_t new_cp = transform(it.code_point);
        libpstr_status_t status = builder_append_utf8(&sb, new_cp);
        if (status != LIBPSTR_OK) {
            builder_cleanup(&sb);
            return status;
        }
    }

    if (sb.vec.len == original->len) {
        memcpy(original->buf, sb.vec.data, original->len);
        original->buf[original->len] = '\0';
        builder_cleanup(&sb);
    } else {
        libpstr_pstr_t *new_s = builder_build(&sb);
        builder_cleanup(&sb);
        if (!new_s) return LIBPSTR_ERR_OOM;
        free(original);
        *self = new_s;
    }
    return LIBPSTR_OK;
}

/* ✨ NEW OWNED UTILITY: ASCII uppercase converter wrapper */
static libpstr_status_t pstr_to_uppercase(libpstr_pstr_t *s) {
    if (!s) return LIBPSTR_ERR_NULL;
    for (size_t i = 0; i < s->len; i++) {
        s->buf[i] = (char)toupper((unsigned char)s->buf[i]);
    }
    return LIBPSTR_OK;
}

/* ✨ NEW OWNED UTILITY: ASCII lowercase converter wrapper */
static libpstr_status_t pstr_to_lowercase(libpstr_pstr_t *s) {
    if (!s) return LIBPSTR_ERR_NULL;
    for (size_t i = 0; i < s->len; i++) {
        s->buf[i] = (char)tolower((unsigned char)s->buf[i]);
    }
    return LIBPSTR_OK;
}

/* ✨ NEW OWNED UTILITY: Zero-allocation tail token slice extractor */
static libpstr_slice_t pstr_strip_suffix(const libpstr_pstr_t *s, const char *suffix) {
    if (!s || !suffix) return (libpstr_slice_t){NULL, 0};
    size_t slen = suffix ? strlen(suffix) : 0;
    if (slen > s->len) return (libpstr_slice_t){s->buf, s->len};
    if (memcmp(s->buf + (s->len - slen), suffix, slen) == 0) {
        return (libpstr_slice_t){s->buf, s->len - slen};
    }
    return (libpstr_slice_t){s->buf, s->len};
}


static libpstr_slice_t pstr_trim(const libpstr_pstr_t *s) {
    return slice_trim((libpstr_slice_t){.ptr = s->buf, .len = s->len});
}

static bool pstr_starts_with(const libpstr_pstr_t *s, const char* prefix) {
    return slice_starts_with((libpstr_slice_t){.ptr = s->buf, .len = s->len}, prefix);
}

static bool pstr_ends_with(const libpstr_pstr_t *s, const char* suffix) {
    return slice_ends_with((libpstr_slice_t){.ptr = s->buf, .len = s->len}, suffix);
}

static bool pstr_split_once(const libpstr_pstr_t *s, const char *delim, libpstr_slice_t *left, libpstr_slice_t *right) {
    return slice_split_once((libpstr_slice_t){.ptr = s->buf, .len = s->len}, delim, left, right);
}

/* =========================================================================
 * 📦 SUB-NAMESPACE SECTION 6: ZERO-ALLOCATION SLICES (libpstr.slice)
 * ========================================================================= */

static libpstr_slice_t slice_substring(const libpstr_pstr_t *s, size_t start, size_t len) {
    if (!s || start > s->len) {
        PANIC("libpstr.slice.substring: parent pointer NULL or start position out of bounds");
    }
    
    // 🛑 CRITICAL FIX: Trigger a defensive panic if requested length goes out of bounds
    if (start + len > s->len) {
        PANIC("libpstr.slice.substring: requested length extends past allocation bounds threshold");
    }
    
    if (!is_char_boundary(s, start) || !is_char_boundary(s, start + len)) {
        PANIC("libpstr.slice.substring: logic boundary failure, coordinates fall inside multi-byte character sequences");
    }
    
    return (libpstr_slice_t){ .ptr = s->buf + start, .len = len };
}

static libpstr_slice_t slice_find_cstr(const libpstr_pstr_t *s, const char *needle) {
    if (!s || !needle) return (libpstr_slice_t){NULL, 0};
    size_t nlen = strlen(needle);
    if (nlen == 0 || nlen > s->len || !utf8_validate(needle, nlen)) {
        return (libpstr_slice_t){NULL, 0};
    }
    char *match = memmem(s->buf, s->len, needle, nlen);
    if (!match) return (libpstr_slice_t){NULL, 0};
    return (libpstr_slice_t){ .ptr = match, .len = nlen };
}

static size_t slice_find_pstr(const libpstr_pstr_t *s, const libpstr_pstr_t *needle) {
    if (!s || !needle || needle->len == 0 || needle->len > s->len) return LIBPSTR_NPOS;
    char* match = memmem(s->buf, s->len, needle->buf, needle->len);
    if (!match) return LIBPSTR_NPOS;

    size_t char_index = 0;
    libpstr_iter_t it;
    iter_init(&it, s);
    while (it.ptr < match && iter_next(&it)) {
        ++char_index;
    }
    return char_index;
}

static size_t slice_byte_to_char(const libpstr_pstr_t *s, size_t byte_idx) {
    if (!s || byte_idx >= s->len) return LIBPSTR_NPOS;
    size_t char_index = 0;
    libpstr_iter_t it;
    iter_init(&it, s);
    while (iter_next(&it)) {
        if ((it.ptr - it.byte_len) > (s->buf + byte_idx)) {
            break;
        }
        if ((it.ptr - it.byte_len) <= (s->buf + byte_idx) && (it.ptr > (s->buf + byte_idx))) {
            return char_index;
        }
        ++char_index;
    }
    return char_index;
}

static libpstr_status_t slice_split(const libpstr_pstr_t *self, const char *delim, libpstr_list_t *out_list) {
    if (!self || !delim || !out_list) return LIBPSTR_ERR_NULL;
    if (!utf8_validate_cstr(delim)) return LIBPSTR_ERR_UTF8;

    size_t delim_len = strlen(delim);
    if (delim_len == 0) return LIBPSTR_ERR_FORMAT;

    size_t start = 0;
    const char *match;
    list_init(out_list);

    while (start < self->len && 
          (match = memmem(self->buf + start, self->len - start, delim, delim_len))) {
        libpstr_slice_t slice = {
            .ptr = self->buf + start,
            .len = (size_t)(match - self->buf) - start
        };
        list_push(out_list, slice);
        start = (size_t)(match - self->buf) + delim_len;
    }
    libpstr_slice_t final = { .ptr = self->buf + start, .len = self->len - start };
    list_push(out_list, final);
    return LIBPSTR_OK;
}

static size_t slice_to_index(const libpstr_pstr_t *s, libpstr_slice_t slice) {
    if (!s || !slice.ptr) return LIBPSTR_NPOS;
    if (slice.ptr < s->buf || slice.ptr >= (s->buf + s->len)) {
        return LIBPSTR_NPOS;
    }
    return (size_t)(slice.ptr - s->buf);
}

/* ✨ UPDATED: Structural whitespace trimmer with strict UTF-8 validation */
static libpstr_slice_t slice_trim(libpstr_slice_t slice) {
    if (slice.ptr == NULL || slice.len == 0) return (libpstr_slice_t){NULL, 0};
    
    // Defensive barrier: reject invalid UTF-8 immediately
    if (utf8_validate(slice.ptr, slice.len) == false) {
        return (libpstr_slice_t){NULL, 0};
    }

    const char *p = slice.ptr;
    const char *end = slice.ptr + slice.len;
    const char *start_ptr = NULL;
    const char *end_ptr = NULL;
    uint32_t cp;
    int bytes;

    while (p < end) {
        const char *current = p;
        if (slice_decode_step(&p, end, &cp, &bytes) == false) break;
        if (is_pstr_whitespace(cp) == false) {
            start_ptr = current;
            end_ptr = current + bytes;
            break;
        }
        p += bytes;
    }
    if (start_ptr == NULL) return (libpstr_slice_t){slice.ptr, 0};

    while (p < end) {
        const char *current = p;
        if (slice_decode_step(&p, end, &cp, &bytes) == false) break;
        if (is_pstr_whitespace(cp) == false) {
            end_ptr = current + bytes;
        }
        p += bytes;
    }
    return (libpstr_slice_t){ .ptr = start_ptr, .len = (size_t)(end_ptr - start_ptr) };
}

/* ✨ UPDATED: High-speed single delimiter token divider with boundary enforcement */
static bool slice_split_once(libpstr_slice_t src, const char *delim, libpstr_slice_t *left, libpstr_slice_t *right) {
    if (src.ptr == NULL || delim == NULL || left == NULL || right == NULL) return false;
    
    size_t delim_len = strlen(delim);
    if (delim_len == 0 || delim_len > src.len) return false;

    // Defensive barrier: ensure both source and delimiter are sound
    if (utf8_validate(src.ptr, src.len) == false) return false;
    if (utf8_validate_cstr(delim) == false) return false;

    char *match = memmem(src.ptr, src.len, delim, delim_len);
    if (match == NULL) return false;

    size_t match_idx = (size_t)(match - src.ptr);
    
    // 🛑 CRITICAL FIX: Ensure the match didn't bisect a multi-byte character sequence
    if (is_char_boundary_at_ptr(src.ptr, src.len, match_idx) == false ||
        is_char_boundary_at_ptr(src.ptr, src.len, match_idx + delim_len) == false) {
        return false;
    }

    left->ptr = src.ptr;
    left->len = match_idx;
    right->ptr = match + delim_len;
    right->len = src.len - left->len - delim_len;
    return true;
}

/* ✨ UPDATED: Token segment lookahead boundary assertion */
static bool slice_starts_with(libpstr_slice_t slice, const char *prefix) {
    if (slice.ptr == NULL || prefix == NULL) return false;
    size_t plen = strlen(prefix);
    if (plen > slice.len) return false;
    
    // Defensive barrier
    if (utf8_validate(slice.ptr, slice.len) == false) return false;
    if (utf8_validate_cstr(prefix) == false) return false;

    return memcmp(slice.ptr, prefix, plen) == 0;
}

/* ✨ UPDATED: Token segment tail lookbehind boundary assertion */
static bool slice_ends_with(libpstr_slice_t slice, const char *suffix) {
    if (slice.ptr == NULL || suffix == NULL) return false;
    size_t slen = strlen(suffix);
    if (slen > slice.len) return false;
    
    // Defensive barrier
    if (utf8_validate(slice.ptr, slice.len) == false) return false;
    if (utf8_validate_cstr(suffix) == false) return false;

    return memcmp(slice.ptr + (slice.len - slen), suffix, slen) == 0;
}

static const char* version_impl(void) {
    return LIBPSTR_VERSION;
}

/* =========================================================================
 * 👑 THE MONOLITHIC STRUCT INITIALIZER MAP BINDING CONTRACT
 * ========================================================================= */

const libpstr_module_t libpstr = {
    .version = version_impl,
    .pstr = {
        .alloc = pstr_alloc,
        .free = pstr_free_str,
        .from_cstr = pstr_from_cstr,
        .from_slice = pstr_from_slice,
        .from_utf8_lossy = pstr_from_utf8_lossy,
        .format = pstr_format,
        .format_v = pstr_format_v,
        .read_from_file = pstr_read_from_file,
        .read_from_file_lossy = pstr_read_from_file_lossy,
        .transform = pstr_transform,
        .to_uppercase = pstr_to_uppercase,
        .to_lowercase = pstr_to_lowercase,
        .strip_suffix = pstr_strip_suffix,
        .trim = pstr_trim,
        .starts_with = pstr_starts_with,
        .ends_with = pstr_ends_with,
        .split_once = pstr_split_once,
    },
    .slice = {
        .substring = slice_substring,
        .find_cstr = slice_find_cstr,
        .find_pstr = slice_find_pstr,
        .byte_to_char = slice_byte_to_char,
        .split = slice_split,
        .slice_to_index = slice_to_index,
        .trim = slice_trim,
        .split_once = slice_split_once,
        .starts_with = slice_starts_with,
        .ends_with = slice_ends_with
    },
    .builder = {
        .init = builder_init,
        .cleanup = builder_cleanup,
        .build = builder_build,
        .append = builder_append,
        .append_cstr = builder_append_cstr,
        .append_pstr = builder_append_pstr,
        .appendf = builder_appendf,
        .vappendf = builder_vappendf,
        .append_utf8 = builder_append_utf8,
        .append_substring = builder_append_substring,
        .replace_range = builder_replace_range,
        .replace_range_cstr = builder_replace_range_cstr,
        .replace_range_pstr = builder_replace_range_pstr,
        .find_cstr = builder_find_cstr,
        .read_line = builder_read_line,
        .read_line_lossy = builder_read_line_lossy
    },
    .utf8 = {
        .iter = {
            .init = iter_init,
            .next = iter_next
        },
        .len = utf8_len,
        .is_char_boundary = is_char_boundary,
        .is_char_boundary_at_ptr = is_char_boundary_at_ptr,
        .validate = utf8_validate,
        .validate_cstr = utf8_validate_cstr,
        .validate_pstr = utf8_validate_pstr
    },
    .vec = {
        .init = vec_init,
        .init_external = vec_init_external,
        .free = vec_free,
        .reserve = vec_reserve,
        .resize = vec_resize,
        .push = vec_push,
        .pop = vec_pop,
        .remove = vec_remove,
        .swap_remove = vec_swap_remove,
        .extend = vec_extend
    },
    .list = {
        .init = list_init,
        .free = list_free,
        .len = list_len,
        .get = list_get,
        .push = list_push,
        .pop = list_pop
    }
};
