#pragma once
#ifndef CPACK_EMITTER_H
#define CPACK_EMITTER_H

#include "manifest.h"
#include "vec_cpack_dependency.h"
#include "vec_pstr_ptr.h" // Include our brand new generated vector wrapper!

/**
 * @brief Generates a production-grade build.ninja script based on parsed project metrics.
 * @param manifest_path Destination filename to write out (typically "build.ninja").
 * @param meta Pointer to the parsed package metadata block.
 * @param deps Pointer to the parsed dependencies tracking array.
 * @param source_files Pointer to the array of dynamically crawled source paths.
 * @return true if the build file was written successfully, false otherwise.
 */
bool cpack_emit_ninja(const char *manifest_path, const cpack_meta_t *meta, 
                      const vec_cpack_dependency_t *deps, const vec_pstr_ptr_t *source_files);

#endif // CPACK_EMITTER_H
