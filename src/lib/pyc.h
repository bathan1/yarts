/**
 * @file pyc.h
 * @brief A python-inspired API for general C scripts.
 */
#pragma once
#include <stdbool.h>
#include <stdlib.h>

/**
 * @brief Get min of A, B, ...
 */
int min(int a, int b);

/**
 * @brief max function
 */
int max(int a, int b);

/* STRING */

/**
 * @brief Buffer pointer + length
 */
struct str {
    char *hd;
    size_t length;
};

/**
 * @brief Static string declaration convenience
 */
#define STR(__s) ((struct str) {.hd=__s, .length=sizeof(__s) - 1})

/**
 * @brief Malloc a char buffer from the format string FMT and wrap it in the returned #str.
 */
struct str str(const char *fmt, ...);

/**
 * @brief Move HD of length LENGTH into a struct #str
 */
struct str strn(char *hd, size_t length);

/**
 * Canonical empty string view. Safe to use everywhere.
 */
extern const struct str STR_EMPTY;

/**
 * @brief Offset string S buffer pointer by 1.
 */
struct str __str_next(struct str s);

/**
 * @brief **Read** S string length.
 * 
 * Length is defined as the "string" length, NOT the buffer size 
 * (which is +1 from the null terminator '\0').
 */
size_t __str_len(struct str s);

/**
 * @brief Get the pointer to the backing byte buffer in S. This is optional for callers because you can just access s.val directly.
 */
char *__str_get(struct str s);

/**
 * @brief Cleanup *dynamically* allocated string S.
 */
bool __str_done(struct str s);

/**
 * @brief Appends char CH directly to S's buffer if IS_MUTABLE. Otherwise, return a *copy* of S 
 */
bool __str_insert(struct str s, char ch);

/**
 * @brief For each char C at index I in S, include C in the returned S if F(C, I) = true
 */
struct str __str_filter(struct str s, bool f (int c, uint i));

/**
 * @brief Map each char C in S to new char F(C).
 */
struct str __str_map(struct str s, int f (int c));

/**
 * @brief Split S against matches of M into N elements.
 */
struct str *__str_split(struct str s, struct str m, size_t *n);

/**
 * @brief Get back S[START:END] (end is exclusive index)
 */
struct str __str_slice(const struct str s, uint start, uint end);

char *__str_to_string(const struct str s);

/* LIST */

/**
 * @brief Doubly linked list
 */
struct list;

/**
 * @brief Get the next node in the list in LS, if it exists.
 */
struct list *__list_next(struct list *ls);

/**
 * @brief Compute number of element in list LS.
 */
size_t __list_len(struct list *ls);

/**
 * @brief Read underlying of node LS.
 */
struct str __list_get(struct list *ls);

/**
 * @brief Cleanup *dynamically* allocated list LS. 
 *        This does NOT free the #str buffers from the nodes,
 *        so YOU are in charge of freeing those.
 */
bool __list_done(struct list *ls);

/**
 * @brief Appends S to the *tail* end of LS *dynamically*.
 */
bool __list_insert(struct list *ls, struct str s);

/**
 * @brief Returns the filtered list head for all nodes that pass F
 *        and writes out the head of the reject list to LS.
 *
 * We maintain a reject list so the caller can be in charge of the reject
 * list's memory. So this is really a partition, but that's a memory-concern 
 * and not meant to be the interface (though you could certainly use it like so).
 */
struct list *__list_filter(struct list **ls, bool f (struct str s, uint i));

/* QUEUE */

/**
 * @brief FIFO over char buffers.
 */
struct queue;

/**
 * @brief Pops the front string of Q.
 */
struct str __queue_next(struct queue *q);

/**
 * @brief Read size of Q;
 */
size_t __queue_len(struct queue *q);

/**
 * @brief Peek the front string of Q.
 */
struct str __queue_get(struct queue *q);

/**
 * @brief Cleanup dynamically allocated Q.
 */
bool __queue_done(struct queue *q);

/**
 * @brief Enqueue S in Q.
 */
bool __queue_insert(struct queue *q, struct str s);

/**
 * @brief Get the empty value for **type** T.
 */
#define empty(T) \
    _Generic((T *)0, \
        struct str *: STR_EMPTY, \
        struct list **: NULL, \
        struct queue **: NULL \
    )

/**
 * @brief Return the next value of ITER. 
 */
#define next(iter) \
    _Generic((iter), \
        struct str: __str_next, \
        struct list *: __list_next, \
        struct queue *: __queue_next \
    )(iter)

/**
 * @brief Read length of ITER.
 */
#define len(iter) \
    _Generic((iter), \
        struct str: __str_len, \
        struct list *: __list_len, \
        struct queue *: __queue_len \
    )(iter)

/**
 * @brief Get the underyling "value" of ITER.
 */
#define hd(iter) \
    _Generic((iter), \
        struct str: __str_get, \
        struct list *: __list_get, \
        struct queue *: __queue_get \
    )(iter)

/**
 * @brief Free dynamically allocated ITER.
 */
#define done(iter) \
    _Generic((iter), \
        struct str: __str_done, \
        struct list *: __list_done, \
        struct queue *: __queue_done \
    )(iter)

/**
 * @brief Return the next value of ITER. 
 */
#define insert(iter, value) \
    _Generic((iter), \
        struct str: __str_insert, \
        struct list *: __list_insert, \
        struct queue *: __queue_insert \
    )((iter), (value))

/**
 * @brief Filter out all elements in ITER that don't pass F.
 */
#define filter(iter, f) \
    _Generic((iter), \
        struct str: __str_filter, \
        struct list **: __list_filter \
    )((iter), (f))

/**
 * @brief F(x) for x in ITER. Memory semantics specific to each struct.
 */
#define map(iter, f) \
    _Generic((iter), \
        struct str: __str_map \
    )((iter), (f))

/**
 * @brief Reduce INIT with each x in ITER in the callback F
 */
#define split(iter, match, f) \
    _Generic((iter), \
        struct str: __str_split \
    )((iter), (match), (f))

