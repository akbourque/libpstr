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

/**
 * @brief A non-owning view of a string segment.
 * Slices are used for efficient string manipulation without allocation.
 */
typedef struct {
    const char* ptr;    ///< Non-owning pointer to the start of a text segment.
    size_t len;         ///< Length of the referenced segment in bytes.
} libpstr_slice_t;

/**
 * @brief A vector of bytes which can transition from a non-owning buffer to the heap.
 * Used by a builder for efficient string manipulation without allocation unless the size exceeds SBO.
 */
typedef struct {
    size_t cap;         ///< Total allocated vector boundary threshold.
    size_t len;         ///< Total number of active bytes currently packed.
    uint8_t* data;      ///< Pointer to internal byte storage.
    bool is_heap;       ///< Flag indicating if allocation dropped onto the heap.
} libpstr_vec_t;

/**
 * @brief A list of slices.
 * Memory for the list is allocated on the heap.
 */
typedef struct {
    libpstr_vec_t vec;  ///< Contiguous byte backing array tracking slice segments side-by-side.
} libpstr_list_t;

/**
 * @brief The core Pascal-style (length-prefixed) string type.
 * Memory is allocated as a single contiguous block containing the header and the buffer.
 */
typedef struct {
    size_t len;         ///< Byte length of string data, excluding null terminator.
    char buf[];         ///< Contiguous UTF-8 encoded flexible array payload buffer.
} libpstr_pstr_t;

/**
 * @brief A p-string builder for efficient creation of p-strings.
 * Uses a buffer on the stack for formatting and appending operations.
 */
typedef struct {
   libpstr_vec_t vec;   ///< Dynamic scratchpad vector frame.
   char sbo[LIBPSTR_BUILDER_SBO]; ///< Contiguous local stack buffer optimization pool.
} libpstr_builder_t;

/**
 * @brief An iterator to iterate over the chars of a UTF-8 string.
 */
typedef struct {
    const char *ptr;    ///< Current character byte evaluation offset pointer index.
    const char *end;    ///< Defensive boundary safety rail threshold anchor.
    uint32_t code_point;///< Decoded active Unicode character glyph payload.
    int byte_len;       ///< Number of bytes consumed by active character representation.
} libpstr_iter_t;


/* --- Encapsulated Sub-Namespace Functional API Interfaces --- */

/** @brief Namespace for owned, heap-allocated Pascal-string operations. */
typedef struct {
    /**
     * @brief Creates a new allocated p-string.
     * @param str The source string.
     * @param len The source string length.
     * @return A new libpstr_pstr_t object, or NULL on OOM, str is NULL, or invalid UTF-8.
     */
    libpstr_pstr_t* (*alloc)(const char* str, size_t len);

    /**
     * @brief Calls free on the libpstr_pstr_t pointer.
     * @param s The p-string.
     */
    void             (*free)(libpstr_pstr_t *s);

    /**
     * @brief Creates a new allocated p-string from a null-terminated C string.
     * @param str The source string.
     * @return A new libpstr_pstr_t object, or NULL on OOM, str is NULL, or invalid UTF-8.
     */
    libpstr_pstr_t* (*from_cstr)(const char* str);

    /**
     * @brief Allocates a new p-string from a libpstr_slice_t.
     * @param slice A slice referencing a valid string buffer.
     * @return A new libpstr_pstr_t object, or NULL on OOM or slice pointer is NULL, or invalid UTF-8.
     */
    libpstr_pstr_t* (*from_slice)(libpstr_slice_t slice);

    /**
     * @brief Returns a newly allocated p-string from a string pointer and length.
     * Replaces invalid chars with the U+FFFD replacement char.
     * @param str The source string.
     * @param len The source string length.
     * @return A new libpstr_pstr_t object, or NULL on OOM or NULL source str.
     */
    libpstr_pstr_t* (*from_utf8_lossy)(const char *str, size_t len);

    /**
     * @brief Returns a newly allocated libpstr_pstr_t from a format specification and data.
     * @param format The format string.
     * @param ... The items to format.
     * @return A newly allocated libpstr_pstr_t.
     */
    libpstr_pstr_t* (*format)(const char* format, ...);

    /**
     * @brief Returns a newly allocated libpstr_pstr_t from a format specification and va_list.
     * @param format The format string.
     * @param args List of items to format.
     * @return A newly allocated libpstr_pstr_t.
     */
    libpstr_pstr_t* (*format_v)(const char* format, va_list args);

    /**
     * @brief Reads an entire file into a new p-string.
     * @param path The filesystem path to the file.
     * @return A new libpstr_pstr_t pointer, or NULL on OOM, file-not-found, or invalid UTF-8 chars.
     */
    libpstr_pstr_t* (*read_from_file)(const char* path);

    /**
     * @brief Reads an entire file into a new p-string, repairing invalid UTF-8.
     * @param path The filesystem path to the file.
     * @return A new libpstr_pstr_t pointer, or NULL on OOM or file-not-found.
     */
    libpstr_pstr_t* (*read_from_file_lossy)(const char* path);

    /**
     * @brief Transforms a string in place by applying a transform function to each code point.
     * @param self The source string.
     * @param transform The transform function: uint32_t transform(uint32_t).
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*transform)(libpstr_pstr_t **self, libpstr_transform_fn transform);
    
    /**
     * @brief Transforms an ASCII string to uppercase in place.
     * @param s The string to transform.
     * @return LIBPSTR_OK on success, or LIBPSTR_ERR_NULL if the string is NULL.
     */
    libpstr_status_t (*to_uppercase)(libpstr_pstr_t *s);

    /**
     * @brief Transforms an ASCII string to lowercase in place.
     * @param s The string to transform.
     * @return LIBPSTR_OK on success, or LIBPSTR_ERR_NULL if the string is NULL.
     */
    libpstr_status_t (*to_lowercase)(libpstr_pstr_t *s);

    /**
     * @brief Returns a zero-allocation slice of the string with the specified suffix removed.
     * @param s The source string.
     * @param suffix The suffix string to remove.
     * @return A libpstr_slice_t pointing to the remaining prefix.
     */
    libpstr_slice_t  (*strip_suffix)(const libpstr_pstr_t *s, const char *suffix);

    /**
     * @brief Creates a view of the string with leading and trailing whitespace removed.
     * @param s The string to trim.
     * @return A libpstr_slice_t pointing to the non-whitespace sub-region.
     */
    libpstr_slice_t  (*trim)(const libpstr_pstr_t *s);

    /**
     * @brief High-speed single delimiter token divider with boundary enforcement.
     * @param src The source string.
     * @param delim The delimiter string to split on.
     * @param left Pointer to store the resulting left slice.
     * @param right Pointer to store the resulting right slice.
     * @return true if the delimiter was found and split successfully, false otherwise.
     */
    bool             (*split_once)(const libpstr_pstr_t *src, const char *delim, libpstr_slice_t *left, libpstr_slice_t *right);

    /**
     * @brief Returns true if a p-string starts with a specified prefix.
     * @param s The source string.
     * @param prefix The prefix string.
     * @return true if s starts with prefix, false otherwise.
     */
    bool             (*starts_with)(const libpstr_pstr_t *s, const char *prefix);

    /**
     * @brief Returns true if a p-string ends with a specified suffix.
     * @param s The source string.
     * @param suffix The suffix string.
     * @return true if s ends with suffix, false otherwise.
     */
    bool             (*ends_with)(const libpstr_pstr_t *s, const char *suffix);
} libpstr_pstr_api;

/** @brief Namespace for high-speed, zero-allocation non-owning slice operations. */
typedef struct {
    /**
     * @brief Extracts a view of a substring without allocation.
     * @param s The source string.
     * @param start The byte offset to begin extraction.
     * @param count The number of bytes to include in the substring.
     * @return A libpstr_slice_t.
     */
    libpstr_slice_t  (*substring)(const libpstr_pstr_t *s, size_t start, size_t count);

    /**
     * @brief Searches for a cstr in a p-string.
     * @param s The source p-string.
     * @param needle The C string to find.
     * @return Returns a libpstr_slice_t with ptr == NULL if not found.
     */
    libpstr_slice_t  (*find_cstr)(const libpstr_pstr_t *s, const char *needle);

    /**
     * @brief Searches for a p-string in a p-string.
     * @param s The source p-string.
     * @param needle The p-string to find.
     * @return A char index, or LIBPSTR_NPOS if not found.
     */
    size_t           (*find_pstr)(const libpstr_pstr_t *s, const libpstr_pstr_t *needle);

    /**
     * @brief Converts a byte index into a p-string to a utf8 char index.
     * @param s The source p-string.
     * @param byte_idx The byte index into s.
     * @return A char index into s or LIBPSTR_NPOS for an invalid byte_idx.
     */
    size_t           (*byte_to_char)(const libpstr_pstr_t *s, size_t byte_idx);

    /**
     * @brief Splits a string into a list of slices based on a delimiter.
     * @param self The source string to split.
     * @param delim The UTF-8 sequence to use as a separator.
     * @param out_list An INITIALIZED list to store the resulting slice elements.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*split)(const libpstr_pstr_t *self, const char *delim, libpstr_list_t *out_list);

    /**
     * @brief Calculates the byte offset of a slice relative to its parent.
     * @param s The source string.
     * @param slice The slice belonging to the source string.
     * @return Byte index, or LIBPSTR_NPOS if the slice does not belong to the string.
     */
    size_t           (*slice_to_index)(const libpstr_pstr_t *s, libpstr_slice_t slice);

    /**
     * @brief Creates a view of the slice with leading and trailing whitespace removed.
     * @param slice The slice to trim.
     * @return A libpstr_slice_t pointing to the non-whitespace sub-region.
     */
    libpstr_slice_t  (*trim)(libpstr_slice_t slice);

    /**
     * @brief High-speed single delimiter token divider for a slice.
     * @param src The source slice.
     * @param delim The delimiter string to split on.
     * @param left Pointer to store the resulting left slice.
     * @param right Pointer to store the resulting right slice.
     * @return true if split successfully, false otherwise.
     */
    bool             (*split_once)(libpstr_slice_t src, const char *delim, libpstr_slice_t *left, libpstr_slice_t *right);

    /**
     * @brief Returns true if a slice starts with a specified prefix.
     * @param slice The source slice.
     * @param prefix The prefix string.
     * @return true if it starts with prefix, false otherwise.
     */
    bool             (*starts_with)(libpstr_slice_t slice, const char *prefix);

    /**
     * @brief Returns true if a slice ends with a specified suffix.
     * @param slice The source slice.
     * @param suffix The suffix string.
     * @return true if it ends with suffix, false otherwise.
     */
    bool             (*ends_with)(libpstr_slice_t slice, const char *suffix);
} libpstr_slice_api;

/** @brief Namespace for stack-buffered text assembly optimization sequences. */
typedef struct {
    /**
     * @brief Initializes a string builder.
     * @param sb The string builder.
     */
    void             (*init)(libpstr_builder_t* sb);

    /**
     * @brief If the buffer allocated heap memory, this releases it and re-initializes the builder.
     * @param sb The string builder.
     */
    void             (*cleanup)(libpstr_builder_t* sb);

    /**
     * @brief Builds a newly allocated p-string using the builder's current buffer.
     * @param b The string builder.
     * @return A new libpstr_pstr_t object or NULL on OOM.
     */
    libpstr_pstr_t* (*build)(libpstr_builder_t* b);

    /**
     * @brief Appends a string to a builder's buffer.
     * @param b The string builder.
     * @param str The source string.
     * @param len The source string length.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*append)(libpstr_builder_t* b, const char* str, size_t len);

    /**
     * @brief Appends a null-terminated C string to a builder's buffer.
     * @param b The string builder.
     * @param str The NTBS string.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*append_cstr)(libpstr_builder_t* b, const char* str);

    /**
     * @brief Appends a p-string to a builder's buffer.
     * @param b The string builder.
     * @param str The p-string.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*append_pstr)(libpstr_builder_t* b, libpstr_pstr_t* str);

    /**
     * @brief Appends a formatted string to a builder's buffer.
     * @param b The string builder.
     * @param format The formatting string.
     * @param ... The formatting variables.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*appendf)(libpstr_builder_t* b, const char* format, ...);

    /**
     * @brief Writes a formatted string to a builder's buffer.
     * @param b The string builder.
     * @param format The format string.
     * @param args List of items to format.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*vappendf)(libpstr_builder_t* b, const char* format, va_list args);

    /**
     * @brief Appends a single unicode char to the builder given its code point.
     * @param sb The string builder.
     * @param cp The code point.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*append_utf8)(libpstr_builder_t *sb, uint32_t cp);

    /**
     * @brief Appends a substring of a p-string to a builder's buffer.
     * @param sb The string builder.
     * @param s The source p-string.
     * @param start The start of the substring.
     * @param count The length of the substring.
     * @return true on success, false on bounds failure.
     */
    bool             (*append_substring)(libpstr_builder_t *sb, const libpstr_pstr_t *s, size_t start, size_t count);

    /**
     * @brief Replaces a slice or range in the builder's buffer with another string.
     * @param sb The string builder.
     * @param start The start of the range to replace.
     * @param len The start range length.
     * @param str The replacement string.
     * @param str_len The replacement string length.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*replace_range)(libpstr_builder_t *sb, size_t start, size_t len, const char *str, size_t str_len);

    /**
     * @brief Replaces a slice or range in the builder's buffer with a C string.
     * @param sb The string builder.
     * @param start The start of the range to replace.
     * @param len The start range length.
     * @param str The replacement NTBS string.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*replace_range_cstr)(libpstr_builder_t *sb, size_t start, size_t len, const char *str);

    /**
     * @brief Replaces a slice or range in the builder's buffer with a p-string.
     * @param sb The string builder.
     * @param start The start of the range to replace.
     * @param len The start range length.
     * @param pstr The replacement p-string.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*replace_range_pstr)(libpstr_builder_t *sb, size_t start, size_t len, const libpstr_pstr_t *pstr);

    /**
     * @brief Searches for a cstr in a builder's buffer.
     * @param b The builder.
     * @param needle The C string to find.
     * @return Returns a libpstr_slice_t with ptr == NULL if not found.
     */
    libpstr_slice_t  (*find_cstr)(libpstr_builder_t* b, const char *needle);

    /**
     * @brief Reads a line from a file into a builder's buffer.
     * @param sb The string builder.
     * @param fp The FILE pointer.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*read_line)(libpstr_builder_t *sb, FILE *fp);

    /**
     * @brief Reads a line from a file into a builder's buffer, repairing invalid UTF-8.
     * @param sb The string builder.
     * @param fp The FILE pointer.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*read_line_lossy)(libpstr_builder_t *sb, FILE *fp); 
} libpstr_builder_api;

/** @brief Namespace for streaming UTF-8 character point iteration boundaries. */
typedef struct {
    /**
     * @brief Initializes a p-string character iterator.
     * @param iter The p-string iterator.
     * @param pstr The p-string to iterate over.
     */
    void (*init)(libpstr_iter_t* iter, const libpstr_pstr_t* pstr);

    /**
     * @brief Moves a p-string iterator to the next UTF-8 character.
     * @param iter The p-string iterator.
     * @return true if advanced to another character, false if at the end.
     */
    bool (*next)(libpstr_iter_t* iter);
} libpstr_iter_api;

/** @brief Namespace for UTF-8 structure semantic validation diagnostics. */
typedef struct {
    libpstr_iter_api iter;

    /**
     * @brief Returns the length of a p-string in UTF-8 characters.
     * @param pstr The p-string.
     * @return The length in characters or 0 for a NULL argument.
     */
    size_t           (*len)(const libpstr_pstr_t* pstr);

    /**
     * @brief Determines if an index into a p-string is on the boundary of a UTF-8 character.
     * @param s The p-string.
     * @param byte_idx An index into the p-string buffer.
     * @return true if the index is on a character boundary.
     */
    bool             (*is_char_boundary)(const libpstr_pstr_t *s, size_t byte_idx);

    /**
     * @brief Determines if a pointer into a char buffer is on the boundary of a UTF-8 character.
     * @param s The character buffer.
     * @param len The buffer's length.
     * @param index A character index into the buffer.
     * @return true if the index is on a character boundary.
     */
    bool             (*is_char_boundary_at_ptr)(const char *s, size_t len, size_t index);

    /**
     * @brief Validates the encoding of a UTF-8 string.
     * @param str The string to validate.
     * @param len The len of the string.
     * @return true if the string is valid UTF-8.
     */
    bool             (*validate)(const char *str, size_t len);

    /**
     * @brief Validates the encoding of a UTF-8 NTSB.
     * @param str The string to validate.
     * @return true if valid UTF-8.
     */
    bool             (*validate_cstr)(const char *str);

    /**
     * @brief Validates the UTF-8 encoding of a p-string.
     * @param str The string to validate.
     * @return true if valid UTF-8.
     */
    bool             (*validate_pstr)(const libpstr_pstr_t *str);
} libpstr_utf8_api;

/** @brief Namespace for raw underlying byte storage primitive vector flows. */
typedef struct {
    /**
     * @brief Initialize a libpstr_vec_t structure.
     * @param v The vector structure.
     * @param capacity The initial capacity of the vector.
     */
    void             (*init)(libpstr_vec_t* v, size_t capacity);

    /**
     * @brief Initialize a vector with an external buffer.
     * @param v The vector structure.
     * @param buf Pointer to an external buffer.
     * @param cap Capacity of the external buffer.
     */
    void             (*init_external)(libpstr_vec_t *v, uint8_t *buf, size_t cap); 

    /**
     * @brief Frees any memory allocated by the vector.
     * @param v The vector.
     */
    void             (*free)(libpstr_vec_t* v);

    /**
     * @brief Increases the vector's capacity by a specified amount.
     * @param v The vector.
     * @param capacity The new capacity to set.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*reserve)(libpstr_vec_t* v, size_t capacity);

    /**
     * @brief Resizes a vector.
     * @param v The vector to resize.
     * @param newsize The new vector size.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*resize)(libpstr_vec_t* v, size_t newsize);

    /**
     * @brief Increases the size of the vector by one byte.
     * @param v The vector.
     * @param byte The byte to push.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*push)(libpstr_vec_t* v, uint8_t byte);

    /**
     * @brief Pops a byte off a vector by reducing its size by 1.
     * @param v The vector.
     */
    void             (*pop)(libpstr_vec_t* v);

    /**
     * @brief Removes the byte at a specified index from a vector.
     * @param v The vector.
     * @param index Index of the byte to remove.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*remove)(libpstr_vec_t* v, size_t index);

    /**
     * @brief Removes the byte at a specified index by swapping it with the last byte.
     * @param v The vector.
     * @param index Index of the byte to remove.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*swap_remove)(libpstr_vec_t* v, size_t index);

    /**
     * @brief Extends the vector's buffer by appending bytes from another buffer.
     * @param v The vector.
     * @param bytes Bytes to append.
     * @param count Number of bytes to append.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*extend)(libpstr_vec_t *v, const uint8_t *bytes, size_t count);
} libpstr_vec_api;

/** @brief Namespace for fast stack slice tracking list arrays. */
typedef struct {
    /**
     * @brief Initialize a list of slices.
     * @param list The list structure.
     */
    void             (*init)(libpstr_list_t *list);

    /**
     * @brief Release the memory allocated by a list.
     * @param list The list structure.
     */
    void             (*free)(libpstr_list_t *list);

    /**
     * @brief Returns the number of slices in the list.
     * @param list The list structure.
     * @return The number of slices.
     */
    size_t           (*len)(const libpstr_list_t* list);

    /**
     * @brief Returns a slice at a specified index in a list.
     * @param list The list structure.
     * @param index Index of the slice.
     * @return A slice structure. Returns an empty slice if out of bounds.
     */
    libpstr_slice_t  (*get)(const libpstr_list_t* list, size_t index);

    /**
     * @brief Pushes a slice onto a list of slices.
     * @param list The list structure.
     * @param slice The slice to push.
     * @return LIBPSTR_OK on success.
     */
    libpstr_status_t (*push)(libpstr_list_t *list, libpstr_slice_t slice);

    /**
     * @brief Removes and returns the last slice in the list.
     * @param list The list structure.
     * @return The popped slice.
     */
    libpstr_slice_t  (*pop)(libpstr_list_t* list);
} libpstr_list_api;


/* --- The Unified Module Interface Blueprint Specification --- */

/**
 * @brief The main hierarchical interface for the libpstr library.
 * Access these functions via the global 'libpstr' constant. 
 * Example: libpstr.utf8.validate(my_str);
 */
typedef struct {
    /**
     * @brief Returns the library version.
     * @return The version string.
     */
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
