/**
 * @file iter8.h
 * @brief Iterable byte-sized buffer data structures
 */
#pragma once
#include <stddef.h>

/**
 * doubly linked list.
 */
typedef struct list list;

struct list {
    struct list *next;
    struct list *prev;
    char *buffer;
    size_t length;
};

/**
 * Go to the next node in the list in LS, if it exists.
 */
struct list *__list_next(struct list *ls);

const char *__list_peek(struct list *ls);

void __list_done(struct list *ls);

/**
 * @brief Get the next value from ITER.
 *
 * You can also call the `__` prefixed implementation
 * functions directly, so this is really just for typing convenience.
 */
#define next(iter) \
    _Generic((iter), \
        struct list *: __list_next \
    )(iter)

#define peek(iter) \
    _Generic((iter), \
        struct list *: __list_peek \
    )(iter)

#define done(iter) \
    _Generic((iter), \
        struct list *: __list_done \
    )(iter)

