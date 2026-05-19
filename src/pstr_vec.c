#include "libpstr.h"
#include "panic.h"

// Macros for optimization hints
#define unlikely(x) __builtin_expect(!!(x), 0)

void pstr_vec_init(pstr_vec_t *v, size_t initial_cap) {
    v->len = 0;
    v->cap = initial_cap;
    if (initial_cap > 0) {
        v->data = (uint8_t*)malloc(initial_cap);
        if (!v->data) {
            PANIC("pstr.vec.init: OOM during initial allocation");
        }
        v->is_heap = true;
    } else {
        v->data = NULL;
        v->is_heap = false;
    }
}

void pstr_vec_init_external(pstr_vec_t *v, uint8_t *buf, size_t cap) {
    v->len = 0;
    v->cap = cap;
    v->is_heap = false;
    v->data = buf;
}

pstr_status_t pstr_vec_push(pstr_vec_t *v, uint8_t byte) {
    if (pstr_vec_reserve(v, v->len + 1) != PSTR_OK) return PSTR_ERR_OOM;
    v->data[v->len++] = byte;
    return PSTR_OK;
}

pstr_status_t pstr_vec_append(pstr_vec_t *v, const void *src, size_t len) {
    if (!src || len == 0) return PSTR_OK;

    // Use the smart reserve function that handles the SBO -> Heap transition
    if (pstr_vec_reserve(v, v->len + len) != PSTR_OK) {
        return PSTR_ERR_OOM;
    }
    
    memcpy(v->data + v->len, src, len);
    v->len += len;
    return PSTR_OK;
}

pstr_status_t pstr_vec_remove(pstr_vec_t *v, size_t index) {
    if (index >= v->len) return PSTR_ERR_BOUNDS;

    // Shift everything after the index one position to the left
    memmove(v->data + index, v->data + index + 1, v->len - index - 1);
    v->len--;
    return PSTR_OK;
}

void pstr_vec_pop(pstr_vec_t *v) {
    if (v->len > 0) v->len--;
    else PANIC("pstr.vec.pop: attempt to pop from an empty vector");
}

pstr_status_t pstr_vec_resize(pstr_vec_t *v, size_t new_len) {
    if (new_len <= v->len) {
        v->len = new_len; // Just shrink the logical length
        return PSTR_OK;
    }

    // If growing, we must ensure capacity
    if (pstr_vec_reserve(v, new_len) != PSTR_OK) {
        return PSTR_ERR_OOM;
    }

    // Optional: Zero-initialize the new "gap" between old len and new len
    // This is a Rust-like safety best practice
    memset(v->data + v->len, 0, new_len - v->len);
    
    v->len = new_len;
    return PSTR_OK;
}

pstr_status_t pstr_vec_reserve(pstr_vec_t *v, size_t needed) {
    if (v->cap >= needed) return PSTR_OK;

    size_t ncap = (v->cap == 0) ? 128 : v->cap * 2;
    while (ncap < needed) ncap *= 2;

    uint8_t *ndata;
    if (v->is_heap) {
        // We already own heap memory; safe to realloc
        ndata = (uint8_t*)realloc(v->data, ncap);
    } else {
        // We are currently on the stack (SBO). Must move to heap.
        ndata = (uint8_t*)malloc(ncap);
        if (ndata && v->len > 0) {
            memcpy(ndata, v->data, v->len);
        }
        v->is_heap = true; // Mark as heap-owned now
    }

    if (!ndata) return PSTR_ERR_OOM;
    v->data = ndata;
    v->cap = ncap;
    return PSTR_OK;
}

pstr_status_t pstr_vec_swap_remove(pstr_vec_t *v, size_t index) {
    if (index >= v->len) return PSTR_ERR_BOUNDS;
    
    // Move the last byte into the slot of the removed byte
    v->data[index] = v->data[v->len - 1];
    v->len--;
    return PSTR_OK;
}

pstr_status_t pstr_vec_extend(pstr_vec_t *v, const uint8_t *bytes, size_t count) {
    if (pstr_vec_reserve(v, v->len + count) != PSTR_OK) return PSTR_ERR_OOM;
    memcpy(v->data + v->len, bytes, count);
    v->len += count;
    return PSTR_OK;
}

