#pragma once
#ifndef CPACK_PARSER_H
#define CPACK_PARSER_H

#include "vendor/libpstr.h"
#include "manifest.h"
#include "vec_cpack_dependency.h"

/**
 * @brief Enumeration representing the current structural block context within cpack.toml.
 */
typedef enum {
    TOML_STATE_GLOBAL,       /**< Out-of-block context. */
    TOML_STATE_PACKAGE,      /**< Inside the [package] block. */
    TOML_STATE_DEPENDENCIES  /**< Inside the [dependencies] block. */
} toml_state_t;

/**
 * @brief Processes a single raw string line, advancing the state machine and populating metadata.
 * * @param line Immutable pointer to the current line slice structure.
 * @param state Pointer to the persistent tracking state variable.
 * @param meta Pointer to the package metadata destination structure.
 */
void cpack_parse_line(const pstr_t *line, toml_state_t *state, cpack_meta_t *meta, vec_cpack_dependency_t *deps); 

#endif // CPACK_PARSER_H
