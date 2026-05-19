#include <assert.h>
#include "libpstr.h"
pstr_status_t pstr_builder_ensure(pstr_builder_t* sb, size_t total);

bool pstr_iter_next(pstr_iter_t *it) {
    if (it->ptr >= it->end) return false;

    unsigned char c = (unsigned char)*it->ptr;
    uint32_t cp = 0;
    int len = 0;

    // Determine length and extract initial bits
    if (c <= 0x7F) { // 0xxxxxxx (ASCII)
        cp = c;
        len = 1;
    } else if ((c & 0xE0) == 0xC0) { // 110xxxxx
        cp = c & 0x1F;
        len = 2;
    } else if ((c & 0xF0) == 0xE0) { // 1110xxxx
        cp = c & 0x0F;
        len = 3;
    } else if ((c & 0xF8) == 0xF0) { // 11110xxx
        cp = c & 0x07;
        len = 4;
    } else {
        return false; // Invalid start byte
    }

    // Safety check: Is there enough data left in the buffer?
    if (it->ptr + len > it->end) return false;

    // Extract bits from continuation bytes (10xxxxxx)
    for (int i = 1; i < len; i++) {
        unsigned char next_c = (unsigned char)it->ptr[i];
        if ((next_c & 0xC0) != 0x80) return false; // Not a continuation byte
        cp = (cp << 6) | (next_c & 0x3F);
    }

    it->code_point = cp;
    it->byte_len = len;
    it->ptr += len; // Advance the pointer for the next call
    return true;
}

pstr_status_t pstr_builder_append_utf8(pstr_builder_t *sb, uint32_t cp) {
    int bytes;
    if (cp <= 0x7F) bytes = 1;
    else if (cp <= 0x7FF) bytes = 2;
    else if (cp <= 0xFFFF) bytes = 3;
    else if (cp <= 0x10FFFF) bytes = 4;
    else return PSTR_ERR_UTF8;

    if (pstr_builder_ensure(sb, sb->vec.len + bytes) != PSTR_OK) return PSTR_ERR_OOM;

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
    return PSTR_OK;
}

// Returns the number of Unicode code points (characters)
size_t pstr_utf8_len(const pstr_t *s) {
    if (!s || s->len == 0) return 0;

    size_t count = 0;
    for (size_t i = 0; i < s->len; i++) {
        // 0xC0 is 11000000 in binary. 0x80 is 10000000.
        // This check skips "continuation bytes".
        if (((unsigned char)s->buf[i] & 0xC0) != 0x80) {
            count++;
        }
    }
    return count;
}

/**
 * Checks if a specific offset in a raw buffer is a UTF-8 character boundary.
 * @param buf The raw byte buffer.
 * @param len The total length of the data in the buffer.
 * @param index The byte offset to check.
 * @return true if index is 0, len, or the start of a UTF-8 code point.
 */
bool pstr_is_char_boundary_at_ptr(const char *buf, size_t len, size_t index) {
    // 0 and len are always valid boundaries for slicing
    if (index == 0 || index == len) return true;
    
    // Out of bounds is never a valid boundary
    if (index > len) return false;

    // A byte is a boundary if it is NOT a continuation byte (bits 10xxxxxx).
    // 0xC0 (11000000) masked with the byte should not equal 0x80 (10000000).
    return ((unsigned char)buf[index] & 0xC0) != 0x80;
}

pstr_slice_t pstr_substring(const pstr_t *s, size_t start, size_t len) {
    if (!s || start > s->len) {
        PANIC("pstr.substring: NULL pstr_t pointer or start index OOB");
    }
    if (start + len > s->len) len = s->len - start;

    // Fix for Issue #1: Enforce boundary safety 
    if (!pstr_is_char_boundary(s, start) || !pstr_is_char_boundary(s, start + len)) {
        PANIC("pstr.substring: indices must be on UTF-8 boundaries");
    }

    return (pstr_slice_t){ .ptr = s->buf + start, .len = len };
}

bool pstr_builder_append_substring(pstr_builder_t *sb, const pstr_t *s, size_t start, size_t len) {
    if (!s) return true;
    if (start + len > s->len) return false;

    // Safety check
    assert(pstr_is_char_boundary(s, start));
    assert(pstr_is_char_boundary(s, start + len));

    return pstr_builder_append(sb, s->buf + start, len);
}

// Internal validation: structural check
bool pstr_utf8_validate(const char *str, size_t len) {
    if (!str) return false;
    const uint8_t *p = (const uint8_t *)str;
    const uint8_t *end = p + len;

    while (p < end) {
        if (*p <= 0x7F) { // 1-byte ASCII
            p++;
        } else if ((*p & 0xE0) == 0xC0) { // 2-byte sequence
            if (p + 1 >= end || (p[1] & 0xC0) != 0x80) return false;
            // Check for overlong encoding (must be > 0x7F)
            if ((*p & 0x1F) < 0x02) return false;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) { // 3-byte sequence
            if (p + 2 >= end || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return false;
            // Check for overlong and surrogates (U+D800 - U+DFFF)
            uint32_t cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            if (cp < 0x0800 || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
            p += 3;
        } else if ((*p & 0xF8) == 0xF0) { // 4-byte sequence
            if (p + 3 >= end || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80) return false;
            uint32_t cp = ((*p & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
            if (cp < 0x10000 || cp > 0x10FFFF) return false;
            p += 4;
        } else {
            return false; // Invalid start byte
        }
    }
    return true;
}


pstr_t* pstr_from_utf8_lossy(const char *str, size_t len) {
    pstr_builder_t sb;
    pstr_builder_init(&sb);
    
    const uint8_t *p = (const uint8_t *)str;
    const uint8_t *end = p + len;

    while (p < end) {
        // Try to find the next valid character
        if (*p <= 0x7F) {
            pstr_builder_append(&sb, (char*)p, 1);
            p++;
        } else {
            // Check if this specific sequence is valid
            // We can reuse a 'dry run' of our validator logic here
            int width = 0;
            if ((*p & 0xE0) == 0xC0) width = 2;
            else if ((*p & 0xF0) == 0xE0) width = 3;
            else if ((*p & 0xF8) == 0xF0) width = 4;

            // Simple validation check for this chunk
            bool valid = (width > 0 && (p + width <= end));
            if (valid) {
                for (int i = 1; i < width; i++) {
                    if ((p[i] & 0xC0) != 0x80) { valid = false; break; }
                }
            }

            if (valid) {
                pstr_builder_append(&sb, (char*)p, width);
                p += width;
            } else {
                // Not valid? Append the replacement character: U+FFFD (0xEF 0xBF 0xBD)
                pstr_builder_append_utf8(&sb, 0xFFFD);
                p++; // Skip only the one bad byte to try and recover
            }
        }
    }
    
    pstr_t *result = pstr_builder_build(&sb);
    pstr_builder_cleanup(&sb);
    return result;
}
