/**
 * @file stream.h
 * @brief In memory streams
 *
 * In memory stream that implements FIFO over a #deque
 */
#pragma once
#include <stdio.h>
#include "cfns.h"

/**
 * @brief Returns a readable FILE handle bound to DEQUE.
 */
FILE *rstream(struct deque8 *deque);

/**
 * @brief Returns a writable FILE handle bound to DEQUE.
 */
FILE *wstream(struct deque8 *deque);

/**
 * `fwrite()` on N or MAX bytes of data (whichever is smaller) from SRC buffer to DST stream.
 */
size_t fwrite8(const char *src, size_t n,
               size_t max, FILE *dst);
FILE *cookie();
