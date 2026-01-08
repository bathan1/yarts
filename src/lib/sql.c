#include "sql.h"
#include <asm-generic/errno-base.h>
#include <assert.h>
#include <ctype.h>
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

static int resolve_column_index(struct str *tokens, size_t tokens_len, size_t current_index) {
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
            fprintf(
                stderr,
                "Column \"url\" was expected to be type TEXT,"
                " "
                "not %s",
                map(tokens[TOK_TYPE], toupper).hd
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
    columns[column_id]->name = str(hidden_column_name);
    columns[column_id]->typename = str("text");
    return 0;
}

static bool isnotdquo(int c, uint _i) {return c != '\"';}
static bool isnotsquo(int c, uint _i) {return c != '\'';}

/**
 * Do the given NUM_TOKENS TOKENS have a `GENERATED ALWAYS AS` clause in it?.
 */
static bool is_with_generated_always_as(struct str *tokens, size_t num_tokens) {
    if (num_tokens < 6) {
        return false;
    }
    if (tokens[TOK_CST].length + 1 != sizeof("generated")
        && tokens[TOK_CST_VAL].length + 1 != sizeof("always")
        && tokens[TOK_CST_VAL2].length + 1 != sizeof("as"))
    {
        return false;
    }
    struct str generated_token = map(tokens[TOK_CST], tolower);
    struct str always_token = map(tokens[TOK_CST_VAL], tolower);
    struct str as_token = map(tokens[TOK_CST_VAL2], tolower);
    bool has_generated_always_as = (
        strncmp(generated_token.hd, "generated", 9) == 0
        &&
        strncmp(always_token.hd, "always", 6) == 0
        &&
        strncmp(as_token.hd, "as", 2) == 0
        &&
        tokens[TOK_CST_GEN_VAL].length > 0
    );

    return has_generated_always_as;
}

/**
 * Initialize column definitions and resolve the user's hidden column options, if any,
 * from the table declaration in ARGC and ARGV.
 */
struct column_def *resolve_hidden_columns(int argc, const char *const *argv) {
    struct column_def *cols = calloc(MAX_COL_COUNT, sizeof(struct column_def));
    // static declarations
    cols[0] = (struct column_def) {
        .name = str("url"),
        .typename = str("text"),
        .default_value = str(""),
        .generated_always_as = NULL,
        .generated_always_as_len = 0
    };
    cols[1] = (struct column_def) {
        .name = str("headers"),
        .typename = str("text"),
        .default_value = str(""),
        .generated_always_as = NULL,
        .generated_always_as_len = 0
    };
    cols[2] = (struct column_def) {
        .name = str("body"),
        .typename = str("text"),
        .default_value = str("", 0),
        .generated_always_as = NULL,
        .generated_always_as_len = 0
    };

    for (int i = FETCH_ARGS_OFFSET; i < argc; i++) {
        size_t num_tokens = 0;
        struct str *tokens = split(str(argv[i]), STR(" "), &num_tokens);

        // handle a default url value
        if (tokens[0].length == 3 
            && strncmp(tokens[0].hd, "url", 3) == 0
            && num_tokens == 4
            /* (url, 1), (text, 2), (default, 3), ('some-url', 4) is 4 tokens */
        ) {
            tokens[3] = filter(tokens[3], isnotsquo);
            cols[0].default_value = str(tokens[3].hd);
        }

    }

    return cols;
}

/**
 * Returns whether or not COLNAME matches that of the hidden columns always included
 * in a fetch table.
 */
static bool is_hidden_column(struct str colname) {
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

/**
 * Remove the leading and trailing dquote at TOKENS[TOK_NAME], printing out
 * an error message with the original argument LINE_RAW in context if there is no
 * corresponding trailing dquote.
 */
static int strip_colname_dquotes(struct str *tokens, const char *line_raw) {
    if (tokens[TOK_NAME].hd[tokens[TOK_NAME].length - 1] != '\"') {
        fprintf(stderr, "Open dquote line is missing closing dquote: %s\n", line_raw);
        free(tokens);
        return 1;
    }
    tokens[TOK_NAME] = filter(tokens[TOK_NAME], isnotdquo);
    return 0;
}

struct column_def *parse_column_defs(int argc, const char *const *argv,
                                      size_t *num_columns)
{
    struct column_def *cols = resolve_hidden_columns(argc, argv);

    size_t n_columns = 3;
    for (int i = FETCH_ARGS_OFFSET; i < argc; i++) {
        size_t num_tokens = 0;
        struct str *tokens = split(str(argv[i]), STR(" "), &num_tokens);

        if (is_hidden_column(tokens[TOK_NAME])) {
            continue; // we already handle this in resolve_hidden_columns()
        }

        if (tokens[TOK_NAME].hd[0] == '\"') {
            if (strip_colname_dquotes(tokens, argv[i]) != 0)
                return NULL;
        } else {
            map(tokens[TOK_NAME], tolower);
        }

        // 5 tokens: id int generated always as (...)
        if (is_with_generated_always_as(tokens, num_tokens)) {
            char *expr_raw = tokens[TOK_CST_GEN_VAL].hd;
            size_t expr_len = tokens[TOK_CST_GEN_VAL].length;
            if (expr_len >= 2 && expr_raw[0] == '(' && expr_raw[expr_len - 1] == ')') {
                expr_raw++;           // move start
                expr_len -= 2;        // remove both '(' and ')'
                expr_raw[expr_len] = 0;
            }
            struct str adjusted = {.hd=expr_raw, .length=expr_len};
            struct str arrow_pattern = {.hd = "->", .length = 2};

            size_t npaths = 0;
            struct str *paths = split(adjusted, arrow_pattern, &npaths);

            for (int i = 0; i < npaths; i++) {
                if (paths[i].length > 0 && paths[i].hd[0] == '\'')
                    paths[i] = filter(paths[i], isnotsquo);
            }

            cols[n_columns].generated_always_as = paths;
            cols[n_columns].generated_always_as_len = npaths;
        }


        cols[n_columns].name = tokens[TOK_NAME];
        cols[n_columns].typename = tokens[TOK_TYPE];
        n_columns += 1;

    }

    if (num_columns) {
        *num_columns = n_columns;
    }

    return cols;
}
