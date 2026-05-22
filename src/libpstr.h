#pragma once
#ifndef LIBPSTR_H
#define LIBPSTR_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // Required for high-performance memmem lookup arrays
#endif

#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "panic.h"

/** @brief libpstr official production release indicator. */
#define LIBPSTR_VERSION "0.1.2"

/** @brief Size of stack buffer used for Small Buffer Optimization. */
#define LIBPSTR_BUILDER_SBO 512

/** @brief Operational fallback sentinel for index misses. */
#define LIBPSTR_NPOS ((size_t) - 1)

/** @brief Fixed type layout dimension indicator. */
#define LIBPSTR_SLICE_SIZE sizeof(libpstr_slice_t)

/**
 * @brief Explicit result status codes for library operations.
 */
typedef enum {
    LIBPSTR_OK = 0,        ///< Operation completed successfully.
    LIBPSTR_ERR_OOM = 1,   ///< Failed to allocate memory on the heap.
    LIBPSTR_ERR_UTF8 = 2,  ///< Input contained an invalid UTF-8 sequence.
    LIBPSTR_ERR_BOUNDS = 3,///< Operation attempted to access memory out of bounds.
    LIBPSTR_ERR_NULL = 4,  ///< A required pointer argument was NULL.
    LIBPSTR_ERR_FORMAT = 5,///< Error occurred during printf-style string formatting.
} libpstr_status_t;

/** @brief User-defined unicode character transformation contract callback. */
typedef uint32_t (*libpstr_transform_fn)(uint32_t codepoint);

/* --- Core Transparent Storage Layout Frameworks --- */

typedef struct {
    const char* ptr;    ///< Non-owning pointer to the start of a text segment.
    size_t len;         ///< Length of the referenced segment in bytes.
} libpstr_slice_t;

typedef struct {
    size_t cap;         ///< Total allocated vector boundary threshold.
    size_t len;         ///< Total number of active bytes currently packed.
    uint8_t* data;      ///< Pointer to internal byte storage.
    bool is_heap;       ///< Flag indicating if allocation dropped onto the heap.
} libpstr_vec_t;

typedef struct {
    libpstr_vec_t vec;  ///< Contiguous byte backing array tracking slice segments side-by-side.
} libpstr_list_t;

typedef struct {
    size_t len;         ///< Byte length of string data, excluding null terminator.
    char buf[];         ///< Contiguous UTF-8 encoded flexible array payload buffer.
} libpstr_pstr_t;

typedef struct {
   libpstr_vec_t vec;   ///< Dynamic scratchpad vector frame.
   char sbo[LIBPSTR_BUILDER_SBO]; ///< Contiguous local stack buffer optimization pool.
} libpstr_builder_t;

typedef struct {
    const char *ptr;    ///< Current character byte evaluation offset pointer index.
    const char *end;    ///< Defensive boundary safety rail threshold anchor.
    uint32_t code_point;///< Decoded active Unicode character glyph payload.
    int byte_len;       ///< Number of bytes consumed by active character representation.
} libpstr_iter_t;


/* --- Encapsulated Sub-Namespace Functional API Interfaces --- */

/** @brief Namespace for owned, heap-allocated Pascal-string operations. */
typedef struct {
    libpstr_pstr_t* (*alloc)(const char* str, size_t len);
    void             (*free)(libpstr_pstr_t *s);
    libpstr_pstr_t* (*from_cstr)(const char* str);
    libpstr_pstr_t* (*from_slice)(libpstr_slice_t slice);
    libpstr_pstr_t* (*from_utf8_lossy)(const char *str, size_t len);
    libpstr_pstr_t* (*format)(const char* format, ...);
    libpstr_pstr_t* (*format_v)(const char* format, va_list args);
    libpstr_pstr_t* (*read_from_file)(const char* path);
    libpstr_pstr_t* (*read_from_file_lossy)(const char* path);
    libpstr_status_t (*transform)(libpstr_pstr_t **self, libpstr_transform_fn transform);
    
    // Owned semantic text operators
    libpstr_status_t (*to_uppercase)(libpstr_pstr_t *s);
    libpstr_status_t (*to_lowercase)(libpstr_pstr_t *s);
    libpstr_slice_t  (*strip_suffix)(const libpstr_pstr_t *s, const char *suffix);
    libpstr_slice_t  (*trim)(const libpstr_pstr_t *s);
    bool             (*split_once)(const libpstr_pstr_t *src, const char *delim, libpstr_slice_t *left, libpstr_slice_t *right);
    bool             (*starts_with)(const libpstr_pstr_t *s, const char *prefix);
    bool             (*ends_with)(const libpstr_pstr_t *s, const char *suffix);
} libpstr_pstr_api;

/** @brief Namespace for high-speed, zero-allocation non-owning slice operations. */
typedef struct {
    libpstr_slice_t  (*substring)(const libpstr_pstr_t *s, size_t start, size_t count);
    libpstr_slice_t  (*find_cstr)(const libpstr_pstr_t *s, const char *needle);
    size_t           (*find_pstr)(const libpstr_pstr_t *s, const libpstr_pstr_t *needle);
    size_t           (*byte_to_char)(const libpstr_pstr_t *s, size_t byte_idx);
    libpstr_status_t (*split)(const libpstr_pstr_t *self, const char *delim, libpstr_list_t *out_list);
    size_t           (*slice_to_index)(const libpstr_pstr_t *s, libpstr_slice_t slice);

    // Zero-allocation stack view processing engines
    libpstr_slice_t  (*trim)(libpstr_slice_t slice);
    bool             (*split_once)(libpstr_slice_t src, const char *delim, libpstr_slice_t *left, libpstr_slice_t *right);
    bool             (*starts_with)(libpstr_slice_t slice, const char *prefix);
    bool             (*ends_with)(libpstr_slice_t slice, const char *suffix);
} libpstr_slice_api;

/** @brief Namespace for stack-buffered text assembly optimization sequences. */
typedef struct {
    void             (*init)(libpstr_builder_t* sb);
    void             (*cleanup)(libpstr_builder_t* sb);
    libpstr_pstr_t* (*build)(libpstr_builder_t* b);
    libpstr_status_t (*append)(libpstr_builder_t* b, const char* str, size_t len);
    libpstr_status_t (*append_cstr)(libpstr_builder_t* b, const char* str);
    libpstr_status_t (*append_pstr)(libpstr_builder_t* b, libpstr_pstr_t* str);
    libpstr_status_t (*appendf)(libpstr_builder_t* b, const char* format, ...);
    libpstr_status_t (*vappendf)(libpstr_builder_t* b, const char* format, va_list args);
    libpstr_status_t (*append_utf8)(libpstr_builder_t *sb, uint32_t cp);
    bool             (*append_substring)(libpstr_builder_t *sb, const libpstr_pstr_t *s, size_t start, size_t count);
    libpstr_status_t (*replace_range)(libpstr_builder_t *sb, size_t start, size_t len, const char *str, size_t str_len);
    libpstr_status_t (*replace_range_cstr)(libpstr_builder_t *sb, size_t start, size_t len, const char *str);
    libpstr_status_t (*replace_range_pstr)(libpstr_builder_t *sb, size_t start, size_t len, const libpstr_pstr_t *pstr);
    libpstr_slice_t  (*find_cstr)(libpstr_builder_t* b, const char *needle);
    libpstr_status_t (*read_line)(libpstr_builder_t *sb, FILE *fp);
    libpstr_status_t (*read_line_lossy)(libpstr_builder_t *sb, FILE *fp); 
} libpstr_builder_api;

/** @brief Namespace for streaming UTF-8 character point iteration boundaries. */
typedef struct {
    void (*init)(libpstr_iter_t* iter, const libpstr_pstr_t* pstr);
    bool (*next)(libpstr_iter_t* iter);
} libpstr_iter_api;

/** @brief Namespace for UTF-8 structure semantic validation diagnostics. */
typedef struct {
    libpstr_iter_api iter;
    size_t           (*len)(const libpstr_pstr_t* pstr);
    bool             (*is_char_boundary)(const libpstr_pstr_t *s, size_t byte_idx);
    bool             (*is_char_boundary_at_ptr)(const char *s, size_t len, size_t index);
    bool             (*validate)(const char *str, size_t len);
    bool             (*validate_cstr)(const char *str);
    bool             (*validate_pstr)(const libpstr_pstr_t *str);
} libpstr_utf8_api;

/** @brief Namespace for raw underlying byte storage primitive vector flows. */
typedef struct {
    void             (*init)(libpstr_vec_t* v, size_t capacity);
    void             (*init_external)(libpstr_vec_t *v, uint8_t *buf, size_t cap); 
    void             (*free)(libpstr_vec_t* v);
    libpstr_status_t (*reserve)(libpstr_vec_t* v, size_t capacity);
    libpstr_status_t (*resize)(libpstr_vec_t* v, size_t newsize);
    libpstr_status_t (*push)(libpstr_vec_t* v, uint8_t byte);
    void             (*pop)(libpstr_vec_t* v);
    libpstr_status_t (*remove)(libpstr_vec_t* v, size_t index);
    libpstr_status_t (*swap_remove)(libpstr_vec_t* v, size_t index);
    libpstr_status_t (*extend)(libpstr_vec_t *v, const uint8_t *bytes, size_t count);
} libpstr_vec_api;

/** @brief Namespace for fast stack slice tracking list arrays. */
typedef struct {
    void             (*init)(libpstr_list_t *list);
    void             (*free)(libpstr_list_t *list);
    size_t           (*len)(const libpstr_list_t* list);
    libpstr_slice_t  (*get)(const libpstr_list_t* list, size_t index);
    libpstr_status_t (*push)(libpstr_list_t *list, libpstr_slice_t slice);
    libpstr_slice_t  (*pop)(libpstr_list_t* list);
} libpstr_list_api;


/* --- The Unified Module Interface Blueprint Specification --- */

typedef struct {
    const char* (*version)(void);
    libpstr_pstr_api    pstr;
    libpstr_slice_api   slice;
    libpstr_builder_api builder;
    libpstr_utf8_api    utf8;
    libpstr_vec_api     vec;
    libpstr_list_api    list;
} libpstr_module_t;

// 👑 The single, solitary exported global interface symbol tracking external linkage rules
extern const libpstr_module_t libpstr;

#endif // LIBPSTR_H
