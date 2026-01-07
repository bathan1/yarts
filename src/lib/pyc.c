#include "pyc.h"
#include <stdio.h>
#include <stdlib.h>

const struct str STR_EMPTY = {
    .val = "",
    .length = 0
};

struct list {
    struct list *next;
    struct list *prev;
    struct str val;
};

struct str __str_next(struct str s) {
    if (s.length == 0 || s.val[0] == '\0') {
        return (struct str) {
            .val = "",
            .length = 0
        };
    }

    return (struct str) {
        .val = s.val + 1,
        .length = s.length - 1 
    };
}

size_t __str_len(struct str s) {
    return s.length;
}

const char *__str_get(struct str s) {
    return s.val;
}

bool __str_done(struct str s) {
    int rc = 0;
    if (s.val) {
        free(s.val);
        rc += 1;
    }
    s.length = 0;
    return rc;
}

struct list *__list_next(struct list *ls) {
    if (!ls) {
        return NULL;
    }
    return ls->next;
}

size_t __list_len(struct list *ls) {
    if (!ls) {
        return 0;
    }
    size_t length = 1; // At least size 1 because LS exists
    struct list *next_node = ls->next;
    struct list *prev_node = ls->prev;

    while (next_node) {
        length += 1;
        next_node = next_node->next;
    }

    while (prev_node) {
        length += 1;
        prev_node = prev_node->prev;
    }
    return length;
}

struct str __list_get(struct list *ls) {
    if (!ls) {
        return empty(str);
    }
    return ls->val;
}

bool __list_done(struct list *ls) {
    int rc = 0;
    if (!ls) {
        return rc;
    }

    struct list *next_node = ls->next;
    struct list *prev_node = ls->prev;

    free(ls);
    rc += 1;

    while (next_node) {
        struct list *next_next = next_node->next;
        free(next_node);
        rc += 1;
        next_node = next_next;
    }

    while (prev_node) {
        struct list *prev_prev = prev_node->prev;
        free(prev_node);
        rc += 1;
        prev_node = prev_prev;
    }

    return rc;
}

