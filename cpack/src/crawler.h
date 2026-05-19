#pragma once
#ifndef CPACK_CRAWLER_H
#define CPACK_CRAWLER_H

#include "vendor/libpstr.h"
#include "vec_pstr_ptr.h"

// Assuming your libpstr library includes a vector type for strings (like pstr_vec_t)
// or we can pass an array/vector structure to collect found .c file paths.

/**
 * @brief Recursively traverses a target directory tree to discover all active .c source files.
 * @param base_dir The starting folder path to crawl (e.g., "src").
 * @param out_files Pointer to an initialized vector where discovered source paths are appended.
 */
void cpack_crawl_source_tree(const char *base_dir, vec_pstr_ptr_t *out_files);

#endif // CPACK_CRAWLER_H
