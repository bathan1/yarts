/**
 * @file sql.h
 * @brief SQL related functions including a basic parser
 */

#pragma once

#include "pyc.h"

struct column_def {
    struct str name;
    struct str typename;
    struct str default_value;

    struct str *generated_always_as;
    size_t generated_always_as_len;
};

/**
 * Allocate the #column_def from user ARGC and ARGV, optionally writing out the number
 * resolved columns to NUM_COLUMNS if it isn't NULL.
 */
struct column_def *parse_column_defs(int argc, const char *const *argv,
                                     size_t *num_columns);
