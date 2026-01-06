#pragma once

/**
 * @file cfns.h
 * @brief Common C functions for the other modules
 */

#include <stdbool.h>
#include <stddef.h>

/// String helpers

/**
 * @brief Buffer and length
 *
 * So you don't have to \c strlen() / remember to store the size
 * if you don't want to recompute.
 */
struct string {
    char *hd;
    size_t length;
};

/**
 * @brief Pack S of *length* (not including null terminator) LEN into a ptr + length struct.
 */
struct string ss(const char *s, size_t len);

/**
 * @brief Allocate a string with the given format string FMT.
 *
 * It's like \c sprintf() except that it *returns* the buffer rather than making you pass it in.
 * FMT can be freed after this.
 *
 * @retval SOME_BUFFER OK. Wrote out successfully.
 * @retval NULL Out of heap memory.
 *
 * # Errors
 * - `EINVAL` - Malformed format string FMT.
 * - `ENOMEM` - Out of memory.
 */
struct string dynamic(const char *fmt, ...);

/**
 * @brief Convert S into lowercase in place.
 *
 * @retval 0 On success, all characters are lowercased.
 * @retval 1 On failure, S couldn't be resolved.
 */
int lowercase(struct string *s);

/**
 * @brief Immutable version of #lowercase(), so S isn't touched.
 */
struct string to_lowercase(const struct string s);

/**
 * @brief Remove all occurrences of CH from S.
 *
 * @retval 0 On success, all instances of CH removed.
 * @retval 1 On failure, S couldn't be resolved.
 */
int rmch(struct string *s, char ch);

/**
 * @brief Slice a *copy* of S from START to END (exclusive).
 */
struct string slice(const struct string s, size_t start, size_t end);

/**
 * @brief Duplicate S and its #string::hd.
 */
struct string stringdup(const struct string s);

/**
 * @brief Split S wherever PATTERN occurs into NTOKENS tokens.
 */
struct string *split(const struct string s, const struct string pattern, size_t *ntokens);

/**
 * @brief Split S wherever DELIM is into NTOKS tokens.
 */
struct string *splitch(const struct string s, char delim, size_t *ntoks);

/**
 * @brief Convert S into uppercase in place.
 *
 * @retval 0 On success, all characters are uppercase.
 * @retval 1 On failure, S couldn't be resolved.
 */
int uppercase(struct string *s);

/**
 * @brief Immutable version of #uppercase(), so S isn't touched.
 */
struct string uppercase_im(const struct string s);

/// Deque declarations

/**
 * @brief A double ended queue for uint8 pointers (aka char).
 *
 * Reads are constant-time from both its head and its tail, so it's technically a "deque".
 */
struct deque8 {
    /** The actual buffer. */
    char **buffer;

    /** #buffer capacity, i.e. read/write at `BUFFER[x >= CAP]` is UB. */
    unsigned long cap;

    /** First in end. */
    unsigned long hd;

    /** Last in end. */
    unsigned long tl;

    /** Stored size. Is updated dynamically from calls to #bassoon_pop. */
    unsigned long count;
};

/**
 * @brief Initializes the dequeue at DEQUE.
 *
 * Allocates the #deque::buffer at DEQUE on the heap, along
 * with setting the initial fields.
 */
void deque8_init(struct deque8 *deque);

/**
 * @brief Push object buffer at VAL into DEQUE.
 */
void deque8_push(struct deque8 *deque, char *val);

/**
 * @brief Free the queue buffer at DEQUE.
 *
 * Also frees DEQUE, so accessing DEQUE after calling
 * #deque8_free() is UB.
 */
void deque8_free(struct deque8 *deque);

/**
 * @brief Pop an object from the queue in DEQUE.
 *
 * #deque8::count is decremented accordingly assuming
 * the queue is nonempty.
 *
 * @retval NULL Empty. `BASS->count = 0`.
 * @retval ~0 Success (anything but 0 / NULL).
 */
char *deque8_pop(struct deque8 *deque);

/** @brief min macro */
#define MIN(a, b) ((a < b) ? a : b)
/** @brief max macro */
#define MAX(a, b) ((a > b) ? a : b)

/**
 * @brief Call perror(TAG), run variadic cleanup code, then return RC
 */
#define perror_rc(__rc, __tag, ...)                              \
    (perror(__tag), __VA_ARGS__, (__rc))
