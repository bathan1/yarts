/**
 * @file sql.h
 * @brief SQL related functions including a basic parser
 */

#pragma once

#include "cfns.h"

struct column_def {
    struct string name;
    struct string typename;
    struct string default_value;

    struct string *generated_always_as;
    size_t generated_always_as_len;
};

struct column_def **column_defs_from_declrs(int argc, const char *const *argv, size_t *num_columns);

/**
 * Allocate the #column_def from user ARGC and ARGV, optionally writing out the number
 * resolved columns to NUM_COLUMNS if it isn't NULL.
 */
struct column_def *parse_column_defs(int argc, const char *const *argv,
                                     size_t *num_columns);
