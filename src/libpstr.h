#pragma once
#ifndef LIBPSTR_H
#define LIBPSTR_H
#ifndef _GNU_SOURCE
// for memmem on some platforms
#define _GNU_SOURCE  
#endif
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "panic.h"

/** @brief libpstr version. */
#define LIBPSTR_VERSION "0.1.0"

/** @brief Size of the stack-allocated buffer used by builder for Small Buffer Optimization. */
#define PSTR_BUILDER_SBO 512

/** @brief Value returned by some functions which return a size_t to indicate an error
 * or a failed operation. */
#define PSTR_NPOS ((size_t) - 1)

/** @brief Size of a pstr_slice_t structure. */
#define PSTR_SLICE_SIZE sizeof(pstr_slice_t)

/**
 * @brief Result codes for libpstr operations.
 */
typedef enum {
    PSTR_OK = 0,        ///< Operation completed successfully.
    PSTR_ERR_OOM = 1,   ///< Failed to allocate memory on the heap.
    PSTR_ERR_UTF8 = 2,  ///< Input contained an invalid UTF-8 sequence.
    PSTR_ERR_BOUNDS = 3,///< Operation attempted to access memory out of bounds.
    PSTR_ERR_NULL = 4,  ///< A required pointer argument was NULL.
    PSTR_ERR_FORMAT = 5,///< Error occurred during string formatting (printf-style).
} pstr_status_t;

/**
 * @brief A function which transforms a unicode code point.
 *
 */
typedef uint32_t (*pstr_transform_fn)(uint32_t codepoint);

/**
 * @brief A non-owning view of a string segment.
 * * Slices are used for efficient string manipulation without allocation.
 * They are frequently returned by functions like pstr.trim() and pstr.split().
 */
typedef struct {
    const char* ptr; ///< Pointer to the start of the string segment.
    size_t len;      ///< Length of the segment in bytes.
} pstr_slice_t;

/**
 * @brief A vector of bytes which can transition from a non-owning buffer to the heap.
 * * The pstr_vec_t type is used by a builder for efficient string manipulation without
 * allocation. Unless the string size becomes greater than PSTR_BUFFER_SBO. The vec will
 * then tranfer it's data to the heap.
 * 
 * @note Always use pstr.vec.free to release. All of vec functions are accessible
 * via @ref pstr.vec.
 * */
typedef struct {
    size_t cap;    ///< Allocated capacity
    size_t len;    ///< Number of bytes in use
    uint8_t* data; ///< Raw byte storage
    bool is_heap;  ///< Data is pointing to heap
} pstr_vec_t;

/**
 * @brief A list of slices.
 * * Memory for the list is allocated on the heap. Always use pstr.list.free to release.
 * @note All list functions are accessible via @ref pstr.list.
 */
typedef struct {
    pstr_vec_t vec; ///< Internal byte storage
} pstr_list_t;

/**
 * @brief The core Pascal-style(length prefixed) string type.
 * * Memory for pstr_t is allocated as a single contiguous block containing
 * the header and the buffer. Always use pstr.free() to release.
 * * @note This struct uses a flexible array member for the buffer.
 */
typedef struct {
    size_t len;     ///< Length of the string in bytes, excluding null terminator.
    char buf[];     ///< The UTF-8 encoded string data.
} pstr_t;

/**
 * @brief A p-string builder for efficient creation of p-strings.
 * * Builder uses a buffer on the stack for formatting and appending operations.
 * Unless the size of the resulting string exceeds PSTR_BUFFER_PSO. After performing
 * these operations, a call to the pstr.builder.build function will return a newly
 * allocated p-string. For very long strings, builder will switch to the heap.
 * 
 * @note In case builder does heap allocation, always use pstr.buffer.free() to release.
 * All builder functions are accessible via @ref pstr.builder.
 * */
typedef struct {
   pstr_vec_t vec; ///< Vector containing string data..
   char sbo[PSTR_BUILDER_SBO]; ///< Buffer used for SBO (small buffer optimization). 
} pstr_builder_t;

/* --- UTF-8 Iterator --- */
/**
 * @brief An iterator to iterate over the chars of a UTF-8 string.
 * @code
 *  pstr_iter_t it;
 *  pstr_iter_init(&it, s); // s is a pstr_t*
 *  int char_count = 0;
 *  while (pstr_iter_next(&it)) {
 *      printf("Character found: U+%04X (size %d bytes)\n", it.code_point, it.byte_len);
 *      char_count++;
 *  }
 * @endcode
**/
typedef struct {
    const char *ptr;      ///< Current position in the buffer
    const char *end;      ///< End of the buffer (safety rail)
    uint32_t code_point;  ///< The decoded Unicode character
    int byte_len;         ///< How many bytes the current character uses
} pstr_iter_t;

/**
 * @brief Creates a new allocated p-string.
 * * @code
 *  const char* str = "I 🚀 C";
 *  pstr_t* s = pstr.new(str, strlen(str)); // or pstr_new
 *  assert(s);
 *  assert(s->len == strlen(str));
 *  assert(strcmp(s->buf, str) == 0);
 *  assert(pstr.utf8.len(s) == 5); // or pstr_utf8_len
 *  pstr.free(s); // or pstr_free
 *  @endcode
 * @param str The source string.
 * @param len The source string length.
 * @return A new pstr_t object, or NULL on OOM, str is NULL, or invalid UTF-8.
 */
pstr_t* pstr_new(const char *str, size_t len);

/**
 * @brief Calls free on the pstr_t pointer.
 * * @param s The p-string.
 */
static void pstr_free(pstr_t *s);

/**
 * @brief Creates a new allocated p-string from a null terminated C string.
 * * @code
 *  const char* str = "I very 🚀 C";
 *  pstr_t* s = pstr.from_cstr(str); // or pstr_from_cstr
 *  assert(s);
 *  assert(s->len == strlen(str));
 *  assert(pstr.utf8.len(s) == 13); // or pstr_utf8_len
 *  assert(strcmp(s->buf, str) == 0);
 *  pstr.free(s); // or pstr_free
 * @endcode
 * @param str The source string.
 * @return A new pstr_t object, or NULL on OOM, str is NULL, or invalid UTF-8.
 */
static pstr_t* pstr_from_cstr(const char* str);

/**
 * @brief Extracts a view of a substring without allocation.
 * * @param s The source string.
 * @param start The byte offset to begin extraction.
 * @param count The number of bytes to include in the substring.
 * @return A pstr_slice_t.
 * @note **PANIC**: This function will terminate the program if 'start' or 
 * 'start + count' do not fall on valid UTF-8 character boundaries or start exceeds
 * the string size or the string pointer is NULL.
 */
pstr_slice_t pstr_substring(const pstr_t *s, size_t start, size_t count);

/**
 * @brief Searches for a cstr in a p-string.
 * * @param s The source p-string.
 * @param needle The C string to find.
 * @return Returns a pstr_slice_t with pstr_slice_t.ptr == NULL if the string
 * is not found, invalid arguements, and for invalid UTF-8 in needle.
 */
pstr_slice_t pstr_find_cstr(const pstr_t *s, const char *needle);


/**
 * @brief Converts a byte index into a p-string to a utf8 char index.
 * * @param s The source p-string.
 * @param byte_idx The byte index into s.
 * @return An char index into s or NPOS for an invalid byte_inx or s is NULL.
 * @note If byte_inx is not on a character boundary, the index will be for the UTF-8 
 * char that contains byte_inx.
 */
size_t pstr_byte_to_char(const pstr_t *s, size_t byte_idx);


/**
 * @brief Searches for a cstr in a p-string.
 * * @param s The source p-string.
 * @param start The byte offset to begin extraction.
 * @param count The number of bytes to include in the substring.
 * @return A new pstr_t object, or NULL on OOM or invalid bounds.
 * @note **PANIC**: This function will terminate the program if 'start' or 
 * 'start + count' do not fall on valid UTF-8 character boundaries.
 */
size_t pstr_find_pstr(const pstr_t *s, const pstr_t *needle);

/**
 * @brief Returns a newly allocated p-string from a string pointer and length.
 * * @param str The source string.
 * @param len The source string length.
 * @return A new pstr_t object, or NULL on OOM or NULL source str.
 * @note This function will not panic for a string that is not valid UTF-8. It will
 * replace invalid chars with the U+FFFD (0xEF 0xBF 0xBD) replacement char.
 */
pstr_t* pstr_from_utf8_lossy(const char *str, size_t len);


/**
 * @brief Transforms a string in place by applying a transform function to each code point.
 * * @param self The source string.
 * @param transform The transform function: uint32_t transform(uint32_t).
 * @return A status code which will be PSTR_OK on success. If any of the arguments are NULL,
 * PSTR_ERR_NULL is returned. If the tranform function causes the string to resize, a 
 * PSTR_ERR_OOM will be returned for an allocation error. 
 * @note If the string has to be resized, your pstr_t pointer will change. If a resize is not
 * needed, the transformed string is just copied to the original.
 * */
pstr_status_t pstr_transform(pstr_t **self, pstr_transform_fn transform);

/**
 * @brief Splits a string into a list of slices based on a delimiter.
 * * This function is an accumulator; it appends new slices to the end of the 
 * provided list without clearing existing content. For the first call to pstr_split or
 * pstr.split, you must call pstr_list_init or pstr.list.init on the list.
 * * @param self The source string to split.
 * @param delim The UTF-8 sequence to use as a separator.
 * @param out_list An INITIALIZED list to store the resulting pstr_slice_t elements.
 * @return PSTR_OK on success, PSTR_ERR_UTF8 if delim is invalid, or PSTR_ERR_OOM.
 * @note Slices point into the 'self' string. Do not free 'self' while using the list.
 */
pstr_status_t pstr_split(const pstr_t *self, const char *delim, pstr_list_t *out_list);

/**
 * @brief Allocates a new p-string from a pstr_slice_t.
 * * @param s A slice referencing a valid string buffer.
 * @return A new pstr_t object, or NULL on OOM or slice pointer is NULL, or invalid UTF-8.
 */
static pstr_t* pstr_from_slice(pstr_slice_t slice);

/**
 * @brief Returns true if a p-string starts with a specified prefix.
 * * @param s The source string.
 * @param prefix The prefix string.
 * @return If self starts with prefix, true, in all other cases self or prefix NULL, prefix
 * not valid UTF-8, false.
 */
bool pstr_starts_with(const pstr_t *self, const char *prefix);


/**
 * @brief Returns true if a p-string ends with a specified suffix.
 * * @param self The source string.
 * @param suffix The suffix string.
 * @return If self ends with suffix, true, in all other cases self or suffix NULL, suffix
 * not valid UTF-8, false.
 * @note Accessible via pstr.ends_with.
 */
bool pstr_ends_with(const pstr_t *self, const char *suffix); 

/**
 * @brief Returns a newly allocated pstr_t from a format specification and data.
 * * @param format The format string.
 * @param ... The items to format.
 * @return A newly allocated pstr_t. If any formatting or allocation errors occur
 * this pointer will be NULL.
 * @note Accessible via pstr.
 */
pstr_t* pstr_format(const char* format, ...);

/**
 * @brief Returns a newly allocated pstr_t from a format specification and va_list.
 * * @param format The format string.
 * @param va_list List of items to format.
 * @return A newly allocated pstr_t. If any formatting or allocation errors occur
 * this pointer will be NULL.
 * @note Accessible via pstr.
 */
pstr_t* pstr_format_v(const char* format, va_list args);

/**
 * @brief Creates a view of the string with leading and trailing whitespace removed.
 * * Uses an optimized range check to identify ASCII and Unicode whitespace (U+00A0).
 * * @param self The string to trim.
 * @return A pstr_slice_t pointing to the non-whitespace sub-region.
 * @return Returns a {NULL, 0} slice if 'self' is NULL or empty.
 */
pstr_slice_t pstr_trim(const pstr_t* self);


/**
 * @brief Reads an entire file into a new p-string.
 * @param path The filesystem path to the file.
 * @return A new pstr_t pointer, or NULL on OOM or file-not-found or 
 * invalid UTF-8 chars.
 */
pstr_t* pstr_read_from_file(const char* path);

/**
 * @brief Reads an entire file into a new p-string, repairing invalid UTF-8.
 * @param path The filesystem path to the file.
 * @return A new pstr_t pointer, or NULL on OOM or file-not-found.
 * @note Replaces invalid UTF-8 characters with the U+FFFD (0xEF 0xBF 0xBD) replacement char.
 */
pstr_t* pstr_read_from_file_lossy(const char* path);


// --- p-string string builder(sb) Functions ---

/**
 * @brief Initializes a string builder.
 * * @param sb The string builer.
 */
static void pstr_builder_init(pstr_builder_t* sb);

/*
 * @brief Appends a string to a builder's buffer.
 * * @param s The source string.
 * @param len The source string length.
 * @return On success PSTR_OK. A PSTR_ERR_NULL for a NULL str. A PSTR_ERR_UTF8 
 * for an invalid UTF-8 str. A PSTR_ERR_OOM for a memory allocation error.
 */
static pstr_status_t pstr_builder_append(pstr_builder_t* b, const char* str, size_t len);

/**
 * @brief Writes a formatted string to a builder's buffer.
 * * @param format The format string.
 * @param va_list List of items to format.
 * @return On success PSTR_OK. Otherwise, a PSTR_ERR_NULL for a NULL args, a
 * PSTR_ERR_FORMAT for a formating error, and a PSTR_ERR_OOM for an allocation failure.
 * @note Accessible via pstr.builder.
 */
pstr_status_t pstr_builder_vappendf(pstr_builder_t *sb, const char *format, va_list args); 

/**
 * @brief Appends a substring of a p-string to a builders buffer.
 * * @param sb The p-string builder.
 * * @param s The source p-string.
 * * @param start The start of the substring in s.
 * * @param len The length of the substring.
 * @return A new pstr_t object, or NULL on OOM or invalid bounds.
 * @note **PANIC**: This function will terminate the program if 'start' or 
 * 'start + count' do not fall on valid UTF-8 character boundaries.
 */
bool pstr_builder_append_substring(pstr_builder_t *sb, const pstr_t *s, size_t start, size_t len);


/**
 * @brief Appends a null-terminated C string to a builder's buffer.
 * * @param b The p-string builder.
 * @param str The NTBS (null terminated byte string).
 * @return On success, PSTR_OK, otherwise PSTR_ERR_NULL for null arguments,
 * PSTR_ERR_UTF8 for invalid UTF-8, and PSTR_ERR_OOM for allocation errors.
 */
static pstr_status_t pstr_builder_append_cstr(pstr_builder_t* b, const char* str);


/**
 * @brief Appends a p-string to a builder's buffer.
 * * @param b The p-string builder.
 * @param str The p-string.
 * @return On success, PSTR_OK, otherwise PSTR_ERR_NULL for null arguments,
 *  errors.
 */
static pstr_status_t pstr_builder_append_pstr(pstr_builder_t* b, pstr_t* str);


/*
 * @brief Appends a formated string to a builder's buffer.
 * * @param b The p-string builder.
 * @param format The formatting string.
 * @param ... The formatting variables.
 * @return On success PSTR_OK. Otherwise, a PSTR_ERR_NULL for a NULL args, a
 * PSTR_ERR_FORMAT for a formating error, and a PSTR_ERR_OOM for an allocation failure.
 */
pstr_status_t pstr_builder_appendf(pstr_builder_t* b, const char* format, ...);


/**
 * @brief Appends a single uncode char to the builder given its code point.
 * * @param sb The p-string builder.
 * @param cp The code point of a unicode character.
 * @return On success, PSTR_OK. Otherwise PSTR_ERR_OOM for an allocation failure, or
 * PSTR_ERR_UTF8 for an encoding error.
 */
pstr_status_t pstr_builder_append_utf8(pstr_builder_t *sb, uint32_t cp);


/**
 * @brief Builds a newly allocated p-string using the builders current buffer.
 * * @param b The p-string builder.
 * @return A new pstr_t object or NULL on OOM.
 */
static pstr_t* pstr_builder_build(pstr_builder_t* b);


/**
 * @brief If the buffer allocated heap memory, this releases it and re-init's the builder.
 * * @param b The p-string builder.
 */
static void pstr_builder_cleanup(pstr_builder_t* b);

/**
 * @brief Replaces a slice or range in the builder's buffer with another string.
 * * @param sb The p-string builder.
 * @param start The start of the range to replace.
 * @param len The start range length.
 * @param str The replacement NTBS string.
 * @return On success, PSTR_OK. Otherwise PSTR_ERR_OOM for an allocation failure, or
 * PSTR_ERR_UTF8 for an encoding error, or PSTR_ERR_BOUNDS if start + len is greater
 * than the builders buffer length.
 * * @note **PANIC**: This function will terminate the program if 'start' or 
 * 'start + len' do not fall on valid UTF-8 character boundaries.
 */
static pstr_status_t pstr_builder_replace_range_cstr(pstr_builder_t *sb, size_t start, size_t len, const char *str);


/**
 * @brief Reads a line from a file into a builder's buffer.
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
 * @param sb The p-string builder.
 * @param fp The FILE pointer.
 * @return On success, PSTR_OK. Otherwise PSTR_ERR_OOM for an allocation failure, or
 * PSTR_ERR_UTF8 for an encoding error, PSTR_ERR_NULL if any it's arguments are NULL.
 * @note The line will replace previous buffer contents.
 */
pstr_status_t pstr_builder_read_line(pstr_builder_t *sb, FILE *fp); 

/**
 * @brief Reads a line from a file into a builder's buffer.
 * * @param sb The p-string builder.
 * @param fp The FILE pointer.
 * @return On success, PSTR_OK. Otherwise PSTR_ERR_OOM for an allocation failure, or
 * PSTR_ERR_NULL if any it's arguments are NULL.
 * @note The line will replace previous buffer contents. Invalid UTF-8 characters are
 * replaced with the U+FFFD (0xEF 0xBF 0xBD) replacement char
 */
pstr_status_t pstr_builder_read_line_lossy(pstr_builder_t *sb, FILE *fp);


/**
 * @brief Replaces a slice or range in the builder's buffer with another string.
 * * @param sb The p-string builder.
 * @param start The start of the range to replace.
 * @param len The start range length.
 * @param str The replacement p-string.
 * @return On success, PSTR_OK. Otherwise PSTR_ERR_OOM for an allocation failure, or
 * PSTR_ERR_UTF8 for an encoding error, or PSTR_ERR_BOUNDS if start + len is greater
 * than the builders buffer length.
 * * @note **PANIC**: This function will terminate the program if 'start' or 
 * 'start + len' do not fall on valid UTF-8 character boundaries.
 */
static pstr_status_t pstr_builder_replace_range_pstr(pstr_builder_t *sb, size_t start, size_t len, const pstr_t *str);

/**
 * @brief Replaces a slice or range in the builder's buffer with another string.
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
 * @param sb The p-string builder.
 * @param start The start of the range to replace.
 * @param len The start range length.
 * @param str The replacement string.
 * @param str_len The replacement string length.
 * @return On success, PSTR_OK. Otherwise PSTR_ERR_OOM for an allocation failure, or
 * PSTR_ERR_UTF8 for an encoding error, or PSTR_ERR_BOUNDS if start + len is greater
 * than the builders buffer length.
 * * @note **PANIC**: This function will terminate the program if 'start' or 
 * 'start + len' do not fall on valid UTF-8 character boundaries.
 */
pstr_status_t
pstr_builder_replace_range(pstr_builder_t *sb, size_t start, size_t len,
                      const char *replace_with, size_t replace_len);

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
pstr_slice_t pstr_builder_find_cstr(pstr_builder_t *sb, const char *needle);

/**
 * @brief Returns the length of a p-string in UTF-8 characters.
 * * @param s The p-string.
 * @return The length in characters or 0 for a NULL argument.
 */
size_t pstr_utf8_len(const pstr_t *s);

/**
 * @brief Initializes a p-string character iterator.
 * * @param it The p-string iterator.
 * @param s The p-string to interate over.
 */
static void pstr_iter_init(pstr_iter_t *it, const pstr_t *s);

/**
 * @brief Moves a p-string iterator to the next UTF-8 character in the string.
 * * @param it The p-string iterator.
 * @return If another character is in the string, advances the iterator and returns true.
 */
bool pstr_iter_next(pstr_iter_t *it);

/**
 * @brief Validates the encoding of a UTF-8 string.
 * * @param str The string to validate.
 * @param len The len of the string.
 */
bool pstr_utf8_validate(const char *str, size_t len);


/**
 * @brief Validates the encoding of a UTF-8 NTSB.
 * * @param str The string to validate.
 */
static bool pstr_utf8_validate_cstr(const char *str);


/**
 * @brief Validates the UTF-8 encoding of a p-string.
 * * @param str The string to validate.
 */
static bool pstr_utf8_validate_pstr(const pstr_t *str);


/**
 * @brief Determines if a pointer into a char buffer is on the boundary of a UTF-8
 * character.
 * * @param s The character buffer.
 * @param len The buffer's length.
 * @param index A character index into the buffer.
 * @return Returns true if the index is on a character boundary.
 */
bool pstr_is_char_boundary_at_ptr(const char *s, size_t len, size_t index);


/**
 * @brief Determines if an index into a p-string is on the boundary of a UTF-8
 * character.
 * * @param s The p-string.
 * @param byte_inx An index into the p-string buf member.
 * @return Returns true if the index is on a character boundary.
 */
static bool pstr_is_char_boundary(const pstr_t *s, size_t byte_idx);

/**
 * @brief Initialize a pstr_vec_t structure.
 * * @param v The vector structure.
 * @param capacity The initial capacity of the vector.
 */
void pstr_vec_init(pstr_vec_t* v, size_t capacity);

/**
 * @brief Appends bytes to the end of a vector's buffer.
 * * @param v The vector structure.
 * @param src The bytes to append.
 * @param count The number of bytes to append.
 * @return On success, PSTR_OK or a PSTR_ERR_OOM for a memory allocation error.
 */
pstr_status_t pstr_vec_append(pstr_vec_t *v, const void *src, size_t count); 

/**
 * @brief Resizes a vector.
 * * @param v The vector to resize.
 * @param newsize The new vector size.
 * @return On success, PSTR_OK or a PSTR_ERR_OOM for a memory allocation error.
 */
pstr_status_t pstr_vec_resize(pstr_vec_t* v, size_t newsize);

/**
 * @brief Increates the size of the vector by one byte.
 * * @param v The vector.
 * @param byte The byte to push.
 * @return On success, PSTR_OK or a PSTR_ERR_OOM for a memory allocation error.
 */
pstr_status_t pstr_vec_push(pstr_vec_t* v, uint8_t byte);

/**
 * @brief Pops a byte off a vector by reducing it's size by 1.
 * * @param v The vector.
 * @note **PANIC**: This function will terminate the program if the vector is empty.
 */
void pstr_vec_pop(pstr_vec_t *v);

/**
 * @brief Removes the byte at a specified index from a vector.
 * * @param v The vector.
 * @param index Index of the byte to remove.
 * @return On success, PSTR_OK or a PSTR_ERR_BOUNDS if index is invalid.
 */
pstr_status_t pstr_vec_remove(pstr_vec_t* v, size_t index);

/**
 * @brief Removes the byte at a specified index from a vector by swapping it 
 * with the last byte and reducing the vectors length by 1.
 * * @param v The vector.
 * @param index Index of the byte to remove.
 * @return On success, PSTR_OK or a PSTR_ERR_BOUNDS if index is invalid.
 */
pstr_status_t pstr_vec_swap_remove(pstr_vec_t* v, size_t index);

/**
 * @brief Frees any memory allocated by the vector.
 * * @param v The vector.
 */
static void pstr_vec_free(pstr_vec_t* v);

/**
 * @brief Increases the vector's capacity by a specified amount.
 * * @param v The vector.
 * @param capacity The new capacity to set.
 * @return On success, PSTR_OK or a PSTR_ERR_OOM for an allocation failure.
 * A PSTR_ERR_NULL for NULL arguments.
 */
pstr_status_t pstr_vec_reserve(pstr_vec_t *v, size_t capacity);

/**
 * @brief Extends the vector's buffer by appending bytes from another buffer.
 * * @param v The vector.
 * * @param bytes Bytes to append to the vector.
 * * @param count Number of bytes to append.
 * @return On success, PSTR_OK or a PSTR_ERR_OOM for an allocation failure,
 * a PSTR_ERR_NULL for NULL arguments.
 */
pstr_status_t pstr_vec_extend(pstr_vec_t *v, const uint8_t *bytes, size_t count);

/**
 * @brief Initialize a pstr_vec_t structure with an external buffer.
 * * @param v The vector structure.
 * @param buf Pointer to an external buffer.
 * @param cap Capacity of the external buffer.
 * @note The vector will not call free on the external buffer. If the size of
 * the vector ever exceeds the capacity of the external buffer, the vector will
 * allocate a new buffer on the heap and copy the data from the external buffer
 * to the heap. Always call pstr.vec.free after use in case the vector had to
 * allocate on the heap.
 */
void pstr_vec_init_external(pstr_vec_t *v, uint8_t *buf, size_t cap); 

/**
 * @brief Initialize a list of slices.
 * * @param list The list structure.
 */
static void pstr_list_init(pstr_list_t *list);

/**
 * @brief Returns the number of slices in the list.
 * * @param list The list structure.
 */
static size_t pstr_list_len(const pstr_list_t *list);

/**
 * @brief Returns a slice at a specified index in a list.
 * * @param list The list structure.
 * @param index Index of the slice.
 * @return A slice structure. If list is NULL or index is OOB, an
 * empty slice will be returned.
 */
pstr_slice_t pstr_list_get(const pstr_list_t *list, size_t index);

/**
 * @brief Pushes a slice onto a list of slices.
 * * @param list The list structure.
 * @param slice The slice to push.
 * @return On success, PSTR_OK, else PSTR_ERR_OOM for allocation errors.
 */
//static pstr_status_t pstr_list_push(const pstr_list_t *list, pstr_slice_t slice);

/**
 * @brief Release the memory allocated by a list.
 * * @param list The list structure.
 */
static void pstr_list_free(pstr_list_t *list);

/**
 * @struct pstr_vec_functions
 * @brief Namespace for vector specific operations.
 * * This structure groups functions for operations on the pstr_vec_t type.
 * Accessible via @ref pstr.vec.
 */
// --- pstr hierarchical interfaces ---
typedef struct {
    void (*init)(pstr_vec_t* v, size_t capacity);
    char* (*back)(pstr_vec_t*);
    pstr_status_t  (*reserve)(pstr_vec_t*, size_t);
    pstr_status_t (*resize)(pstr_vec_t*, size_t);
    pstr_status_t (*push)(pstr_vec_t*, uint8_t);
    void (*pop)(pstr_vec_t*);
    pstr_status_t (*remove)(pstr_vec_t*, size_t);
    pstr_status_t (*swap_remove)(pstr_vec_t*, size_t);
    void (*free)(pstr_vec_t*);
    pstr_status_t (*extend)(pstr_vec_t *v, const uint8_t *bytes, size_t count);
    void (*init_external)(pstr_vec_t *v, uint8_t *buf, size_t cap); 
    pstr_status_t (*transform)(pstr_t **self, pstr_transform_fn transform); 
} pstr_vec_functions;

/**
 * @struct pstr_builder_functions
 * @brief Namespace for builder specific operations.
 * * This structure groups functions for operations on the pstr_builder_t type.
 * Accessible via @ref pstr.builder.
 */
typedef struct {
    void (*init)(pstr_builder_t* sb);
    void (*cleanup)(pstr_builder_t* sb);
    pstr_status_t (*append)(pstr_builder_t* b, const char* str, size_t len);
    bool (*append_substring)(pstr_builder_t *sb, const pstr_t *s, size_t start, size_t count);
    pstr_status_t (*append_cstr)(pstr_builder_t* b, const char* str);
    pstr_status_t (*append_pstr)(pstr_builder_t* b, pstr_t* str);
    pstr_status_t (*appendf)(pstr_builder_t* b, const char* format, ...);
    pstr_status_t (*append_utf8)(pstr_builder_t *sb, uint32_t cp);
    pstr_t*       (*build)(pstr_builder_t* b);
    pstr_status_t (*replace_range)(pstr_builder_t *sb, size_t start, size_t len, const char *str, size_t str_len);
    pstr_status_t (*replace_range_cstr)(pstr_builder_t *sb, size_t start, size_t len, const char *str);
    pstr_status_t (*replace_range_pstr)(pstr_builder_t *sb, size_t start, size_t len, const pstr_t* pstr);
    pstr_status_t (*vappendf)(pstr_builder_t* b, const char* format, va_list args);
    pstr_slice_t (*find_cstr)(pstr_builder_t* b, const char* needle);
    pstr_status_t (*read_line)(pstr_builder_t *sb, FILE *fp); 
    pstr_status_t (*read_line_lossy)(pstr_builder_t *sb, FILE *fp); 
} pstr_builder_functions;

/**
 * @struct pstr_iter_functions
 * @brief Namespace for iter specific operations.
 * * This structure groups functions for iterating a UTF-8 string by characters. Accessible
 * via @ref pstr.utf8.iter.
 */
typedef struct {
    void (*init)(pstr_iter_t* iter, const pstr_t* pstr);
    bool (*next)(pstr_iter_t* iter);
} pstr_iter_functions;

/**
 * @struct pstr_utf8_functions
 * @brief Namespace for UTF-8 specific operations.
 * * This structure groups functions for validation, iteration, and 
 * character boundary checks. Accessible via @ref pstr.utf8.
 */
typedef struct {
    pstr_iter_functions iter;
    size_t (*len)(const pstr_t* pstr);
    bool (*is_char_boundary)(const pstr_t *s, size_t byte_idx);
    bool (*is_char_boundary_at_ptr)(const char *s, size_t len, size_t index);
    bool (*validate)(const char *str, size_t len);
    bool (*validate_cstr)(const char *str);
    bool (*validate_pstr)(const pstr_t *str);
} pstr_utf8_functions;

/**
 * @struct pstr_list_functions
 * @brief Namespace for list specific operations.
 * * This structure groups functions for operations on pstr_list_t, a list of slices.
 * Accessible via @ref pstr.list.
 */
typedef struct {
    void (*free)(pstr_list_t *list);
    size_t (*len)(const pstr_list_t* list);
    pstr_slice_t (*get)(const pstr_list_t* list, size_t index);
    void (*init)(pstr_list_t *list);
    pstr_status_t (*push)(pstr_list_t *list, pstr_slice_t slice);
    pstr_slice_t (*pop)(pstr_list_t* list);
} pstr_list_functions;

/**
 * @struct pstr_functions
 * @brief The main hierarchical interface for the libpstr library.
 * * Access these functions via the global 'pstr' constant. 
 * Example: pstr.utf8.validate(my_str);
 */
typedef struct {
    pstr_t* (*alloc)(const char* str, size_t len);
    void    (*free)(pstr_t *s);
    pstr_t* (*from_cstr)(const char* str);
    pstr_slice_t (*substring)(const pstr_t *s, size_t start, size_t count);
    pstr_slice_t (*find_cstr)(const pstr_t *s, const char *needle);
    size_t (*byte_to_char)(const pstr_t *s, size_t byte_idx);
    size_t (*find_pstr)(const pstr_t *s, const pstr_t *needle);
    pstr_t* (*from_utf8_lossy)(const char *str, size_t len);
    pstr_status_t (*transform)(pstr_t **self, pstr_transform_fn transform); 
    pstr_status_t (*split)(const pstr_t *self, const char *delim, pstr_list_t *out_list);
    bool (*starts_with)(const pstr_t *self, const char *prefix);
    bool (*ends_with)(const pstr_t *self, const char *suffix);
    pstr_t* (*from_slice)(pstr_slice_t slice);
    size_t (*slice_to_index)(const pstr_t *s, pstr_slice_t slice);
    pstr_slice_t (*trim)(const pstr_t* self);
    const char* (*version)();
    pstr_t* (*format)(const char* format, ...);
    pstr_t* (*format_v)(const char* format, va_list args);
    pstr_t* (*read_from_file)(const char* path);
    pstr_t* (*read_from_file_lossy)(const char* path);
    pstr_builder_functions builder; 
    pstr_utf8_functions utf8;
    pstr_vec_functions vec;
    pstr_list_functions list;
} pstr_functions;

extern const pstr_functions pstr;

// --- Inline Functions ---
static inline void pstr_free(pstr_t* s) {
    free(s);
}

static inline pstr_t* pstr_from_cstr(const char* str) {
    return pstr_new(str, strlen(str));
}

static inline void pstr_builder_init(pstr_builder_t* sb) {
    pstr_vec_init_external(&sb->vec, (uint8_t*)sb->sbo, PSTR_BUILDER_SBO);
    sb->sbo[0] = '\0';
}

static inline pstr_t* pstr_builder_build(pstr_builder_t *sb) {
    return pstr_new((char*)sb->vec.data, sb->vec.len);
}

static inline void pstr_builder_cleanup(pstr_builder_t *sb) {
    // if using heap free and reset buf
    if ((sb != NULL) && sb->vec.is_heap) {
        free(sb->vec.data);
        pstr_builder_init(sb);
    }
}

static inline pstr_status_t pstr_builder_append(pstr_builder_t *sb, const char *str, size_t len) {
    if (!str) return PSTR_ERR_NULL;

    if (!pstr_utf8_validate(str, len)) {
        return PSTR_ERR_UTF8; 
    }

    if (pstr_vec_append(&sb->vec, str, len) != PSTR_OK) {
        return PSTR_ERR_OOM;
    }

    sb->vec.data[sb->vec.len] = '\0';
    return PSTR_OK;
}

static inline pstr_status_t pstr_builder_append_cstr(pstr_builder_t* b, const char* str) {
    if (str == NULL) return PSTR_ERR_NULL;
    return pstr_builder_append(b, str, strlen(str));
}

static inline pstr_status_t pstr_builder_append_pstr(pstr_builder_t* b, pstr_t* str) {
    return pstr_builder_append(b, str->buf, str->len);
}

static inline void pstr_iter_init(pstr_iter_t *it, const pstr_t *s) {
    if (!s) {
        it->ptr = it->end = NULL;
        return;
    }
    it->ptr = s->buf;
    it->end = s->buf + s->len;
    it->code_point = 0;
    it->byte_len = 0;
}

static inline bool pstr_is_char_boundary(const pstr_t *s, size_t index){
    if (index == 0 || index == s->len) return true;
    if (index > s->len) return false;
    return ((unsigned char)s->buf[index] & 0xC0) != 0x80;
}

static inline void pstr_vec_free(pstr_vec_t* v){ 
    if (v && v->data) {
        free(v->data);
        pstr_vec_init(v, 0);
    }
}

static inline pstr_status_t pstr_builder_replace_range_cstr(pstr_builder_t *sb, size_t start, size_t len, const char *str) {
    size_t rlen = str ? strlen(str) : 0;
    return pstr_builder_replace_range(sb, start, len, str, rlen);
}

static inline pstr_status_t pstr_builder_replace_range_pstr(pstr_builder_t *sb, size_t start, size_t len, const pstr_t *ps) {
    if (ps == NULL) return pstr_builder_replace_range(sb, start, len, NULL, 0);
    return pstr_builder_replace_range(sb, start, len, ps->buf, ps->len);
}

static inline bool pstr_utf8_validate_cstr(const char *str) {
    return str ? pstr_utf8_validate(str, strlen(str)) : false;
}

static inline bool pstr_utf8_validate_pstr(const pstr_t *s) {
    return s ? pstr_utf8_validate(s->buf, s->len) : false;
}

static inline void pstr_list_free(pstr_list_t *list) {
    if (!list) return;
    pstr_vec_free(&list->vec);
}

static inline pstr_t* pstr_from_slice(pstr_slice_t slice) {
    if (!slice.ptr) return NULL;
    // Uses existing pstr_new to handle allocation and UTF-8 validation
    return pstr_new(slice.ptr, slice.len);
}

static inline size_t pstr_list_len(const pstr_list_t *list) {
    return list ? list->vec.len/PSTR_SLICE_SIZE : 0;
}

static inline void pstr_list_init(pstr_list_t *list) {
    if (list) {
        pstr_vec_init(&list->vec, 0);
    }
}

/**
 * @brief Calculates the byte offset of a slice relative to its parent pstr_t.
 * @return Byte index, or PSTR_NPOS if the slice does not belong to the pstr_t.
 */
static inline size_t pstr_slice_to_index(const pstr_t *s, pstr_slice_t slice) {
    if (!s || !slice.ptr) return PSTR_NPOS;
    // Pointer arithmetic: ensure the slice actually points inside the pstr buffer
    if (slice.ptr < s->buf || slice.ptr >= (s->buf + s->len)) {
        return PSTR_NPOS;
    }
    return (size_t)(slice.ptr - s->buf);
}
/**
 * @brief Appends a slice to the end of the list.
 */
static inline pstr_status_t pstr_list_push(pstr_list_t *list, pstr_slice_t slice) {
    if (!list) return PSTR_ERR_NULL;
    // We use PSTR_SLICE_SIZE (sizeof(pstr_slice_t)) to ensure we move the right amount of bytes
    return pstr_vec_append(&list->vec, &slice, PSTR_SLICE_SIZE);
}

/**
 * @brief Removes and returns the last slice in the list.
 * @note **PANIC**: Diagnostic abort if the list is empty.
 */
static inline pstr_slice_t pstr_list_pop(pstr_list_t *list) {
    if (!list || list->vec.len < PSTR_SLICE_SIZE) {
        PANIC("pstr.list.pop: attempt to pop from an empty list");
    }
    
    pstr_slice_t slice = pstr_list_get(list, pstr_list_len(list) - 1);
    list->vec.len -= PSTR_SLICE_SIZE;
    return slice;
}

/**
 * @brief Returns the library version.
 */
static inline const char* pstr_version(void) {
    return LIBPSTR_VERSION;
}

#endif // LIBPSTR_H
