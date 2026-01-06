#include "sql.h"
#include <asm-generic/errno-base.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COL_COUNT 64
#define FETCH_ARGS_OFFSET 3

enum {
    // id
    TOK_NAME = 0,
    // int
    TOK_TYPE,
    // generated
    TOK_CST,
    // always
    TOK_CST_VAL,
    // as
    TOK_CST_VAL2,
    // (generated_val)
    TOK_CST_GEN_VAL
};


enum { COL_URL = 0, COL_HEADERS, COL_BODY };

static int resolve_column_index(struct string *tokens, size_t tokens_len, size_t current_index) {
    bool is_url_column = (
        tokens_len > 0 &&
        tokens[TOK_NAME].length == 3 &&
        strncmp(tokens[TOK_NAME].hd, "url", 3) == 0
    );
    if (is_url_column) {
        // check that type is TEXT before returning the index
        if (tokens_len < 2 || tokens[TOK_TYPE].length != 4
            || strncmp(tokens[TOK_TYPE].hd, "text", 4) != 0)
        {
            char *typename = uppercase_im(tokens[TOK_TYPE]).hd;
            fprintf(
                stderr,
                "Column \"url\" was expected to be type TEXT,"
                " "
                "not %s",
                typename
            );
            return -1;
        }

        return COL_URL;
    }

    return current_index;
}

// Write out hidden column with code INDEX to COLUMNS
// if COLUMNS[INDEX] == NULL.
// 0 on success, 1 on fail with \c errno set
static int hidden_column(int column_id, struct column_def **columns) {
    assert(columns && "COLUMNS can't be NULL");
    if (columns[column_id] != NULL) {
        errno = EEXIST;
        return 1;
    }

    columns[column_id] = calloc(1, sizeof(struct column_def));
    if (!columns[column_id]) // ENOMEM
        return 1;
    char hidden_column_name[8] = {0}; // at most can be "headers"
    switch (column_id) {
        case COL_URL:
            memcpy(hidden_column_name, "url", 3);
            break;
        case COL_HEADERS:
            memcpy(hidden_column_name, "headers", 7);
            break;
        case COL_BODY:
            memcpy(hidden_column_name, "body", 4);
            break;
    }
    columns[column_id]->name = ss(hidden_column_name, 3);
    columns[column_id]->typename = ss(
        strndup("text", 4),
        3
    );
    return 0;
}

struct column_def **column_defs_from_declrs(int argc, const char *const *argv,
                                          size_t *num_columns)
{
    struct column_def *columns[64 + 3] = {0};
    // columns always has [URL, HEADERS, BODY] at start
    size_t col_index = 3;

    for (int i = FETCH_ARGS_OFFSET; i < argc; i++) {
        bool is_url_column = false, has_default = false, has_generated_value = false;
        // Each argument makes up 1 column declaration
        const char *declaration = argv[i];
        size_t tokens_size = 0;
        struct string *tokens = splitch(
            (struct string) {.hd=(char *) declaration, .length=strlen(declaration)},
            ' ',
            &tokens_size
        );
        if (!tokens || tokens_size < 1) {
            return NULL;
        }

        // 0   1    2       3      4  5
        // id int default   0
        // id int generated always as ()
        for (int i = 1; i < MIN(5, tokens_size); i++) {
            if (tokens[i].hd[0] != '\'') {
                if (lowercase(&tokens[i]) != 0) {
                    for (int i = 0; i < tokens_size; i++) free(tokens[i].hd);
                    free(tokens);
                    return NULL;
                }
            }
        }

        const char *column_name = tokens[TOK_NAME].hd;
        if (column_name[TOK_NAME] == '\"') {
            if (column_name[tokens[TOK_NAME].length - 1] != '\"') {
                fprintf(stderr, "Open dquote missing closing dquote in column name %s\n", column_name);
                for (int t = 0; t < tokens_size; t++) free(tokens[t].hd);
                free(tokens);
                return NULL;
            }
            if (rmch(&tokens[TOK_NAME], '\"') != 0) {
                for (int t = 0; t < tokens_size; t++) free(tokens[t].hd);
                free(tokens);
                return perror_rc(NULL, "rmch", 0);
            }
        }

        if (tokens_size >= 3 &&
            tokens[TOK_CST].length == 7 &&
                strncmp(tokens[TOK_CST].hd, "default", 7) == 0)
        {
            if (tokens_size != 4) {
                if (tokens_size < 4) {
                    fprintf(stderr, "Default value missing for column %s\n", tokens[TOK_NAME].hd);
                } else {
                    fprintf(stderr, "Too many arguments for default value of column %s\n", tokens[TOK_NAME].hd);
                }
                for (int t = 0; t < tokens_size; t++) free(tokens[t].hd);
                free(tokens);
                return NULL;
            }
            has_default = true;
        }

        if (tokens_size >= 3 &&
            tokens[TOK_CST].length == 9 &&
            strncmp(tokens[TOK_CST].hd, "generated", 9) == 0
            &&
            tokens[TOK_CST_VAL].length == 6 &&
            strncmp(tokens[TOK_CST_VAL].hd, "always", 6) == 0
            &&
            tokens[TOK_CST_VAL2].length == 2 &&
            strncmp(tokens[TOK_CST_VAL2].hd, "as", 2) == 0
            &&
            tokens[TOK_CST_GEN_VAL].length > 0
        ) {
            has_generated_value = true;
        }

        int icol = resolve_column_index(tokens, tokens_size, i);
        if (icol < 0) {
            for (int t = 0; t < tokens_size; t++) free(tokens[t].hd);
            free(tokens);
            return NULL;
        }

        columns[icol] = calloc(1, sizeof(struct column_def));
        if (!columns[icol]) {
            for (int t = 0; t < tokens_size; t++) free(tokens[t].hd);
            free(tokens);
            return NULL;
        }

        columns[icol]->name = tokens[TOK_NAME];
        columns[icol]->typename = tokens[TOK_TYPE];
        if (has_default) {
            if (rmch(&tokens[TOK_CST_VAL], '\'') != 0) {
                for (int t = 0; t < tokens_size; t++) free(tokens[t].hd);
                free(tokens);
                return NULL;
            }
            columns[icol]->default_value = tokens[TOK_CST_VAL];
        }
        if (has_generated_value) {
            char *expr_raw = tokens[TOK_CST_GEN_VAL].hd;
            size_t expr_len = tokens[TOK_CST_GEN_VAL].length;
            if (expr_len >= 2 && expr_raw[0] == '(' && expr_raw[expr_len - 1] == ')') {
                expr_raw++;           // move start
                expr_len -= 2;        // remove both '(' and ')'
            }
            struct string adjusted = {.hd=expr_raw, .length=expr_len};
            struct string arrow_pattern = {.hd = "->", .length = 2};

            size_t path_count = 0;
            struct string *paths = split(adjusted, arrow_pattern, &path_count);
            if (!paths) {
                for (int t = 0; t < tokens_size; t++) free(tokens[t].hd);
                free(tokens);
                return NULL;
            }

            columns[icol]->generated_always_as = paths;
            columns[icol]->generated_always_as_len = path_count;
        }

        // if we're at url, then we wrote to index 0, so we DON'T increment index counter
        col_index = icol < i ? col_index : col_index + 1;
    }

    if (hidden_column(COL_URL, columns) != 0
        || hidden_column(COL_HEADERS, columns) != 0
        || hidden_column(COL_BODY, columns) != 0)
    {
        if (errno != EEXIST) {
            return NULL;
        }
        // else the hidden column was explicitly user defined
    }

    struct column_def **heap_columns =
        calloc(col_index, sizeof(struct column_def *));
    if (!heap_columns) return NULL;

    memcpy(heap_columns, columns,
        col_index * sizeof(struct column_def *));

    if (num_columns) *num_columns = col_index;

    return heap_columns;
}

/**
 * Initialize column definitions and resolve the user's hidden column options, if any,
 * from the table declaration in ARGC and ARGV.
 */
struct column_def *resolve_hidden_columns(int argc, const char *const *argv) {
    struct column_def *cols = calloc(MAX_COL_COUNT, sizeof(struct column_def));
    // static declarations
    cols[0] = (struct column_def) {
        .name = ss("url", 4),
        .typename = ss("text", 4),
        .default_value = ss("", 0),
        .generated_always_as = NULL,
        .generated_always_as_len = 0
    };
    cols[1] = (struct column_def) {
        .name = ss("headers", 7),
        .typename = ss("text", 4),
        .default_value = ss("", 0),
        .generated_always_as = NULL,
        .generated_always_as_len = 0
    };
    cols[2] = (struct column_def) {
        .name = ss("body", 7),
        .typename = ss("text", 4),
        .default_value = ss("", 0),
        .generated_always_as = NULL,
        .generated_always_as_len = 0
    };

    for (int i = FETCH_ARGS_OFFSET; i < argc; i++) {
        size_t num_tokens = 0;
        struct string *tokens = splitch(
            (struct string) {
                .hd=(char *) argv[i],
                .length = strlen(argv[i])
            },
            ' ',
            &num_tokens
        );

        if (tokens[0].length == 3 
            && strncmp(tokens[0].hd, "url", 3) == 0
            && num_tokens == 4
            /* (url, 1), (text, 2), (default, 3), ('some-url', 4) is 4 tokens */
        ) {
            rmch(&tokens[3], '\'');
            cols[0].default_value = ss(
                tokens[3].hd,
                tokens[3].length
            );
        }
    }

    return cols;
}

/**
 * Returns whether or not COLNAME matches that of the hidden columns always included
 * in a fetch table.
 */
bool is_hidden_column(struct string colname) {
    if (colname.length + 1 != (sizeof("url"))
        && colname.length + 1 != sizeof("headers")
        && colname.length + 1 != sizeof("body")
    ) {
        // early exit if buffer size isn't one of the hidden column sizes
        return false;
    }

    return (
        (colname.length == 3 && strncmp(colname.hd, "url", 3) == 0)
        || (colname.length == 7 && strncmp(colname.hd, "headers", 7) == 0)
        || (colname.length == 4 && strncmp(colname.hd, "body", 4) == 0)
    );
}

struct column_def *parse_column_defs(int argc, const char *const *argv,
                                      size_t *num_columns)
{
    struct column_def *cols = resolve_hidden_columns(argc, argv);

    size_t n_columns = 3;
    for (int i = FETCH_ARGS_OFFSET; i < argc; i++) {
        size_t num_tokens = 0;
        struct string *tokens = splitch(
            (struct string) {
                .hd = (char *) argv[i],
                .length = strlen(argv[i])
            },
            ' ',
            &num_tokens
        );

        if (is_hidden_column(tokens[0])) {
            // we already handle this in resolve_hidden_columns()
            continue;
        }

        cols[n_columns].name = tokens[0];
        cols[n_columns].typename = tokens[1];
        n_columns += 1;
    }

    if (num_columns) {
        *num_columns = n_columns;
    }

    return cols;
}
