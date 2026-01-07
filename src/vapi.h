/**
 * @file vapi.h
 * @brief Yet Another Runtime TCP Stream internal functions API
 *
 * Helper functions #vttp.c has available.
 *
 * @example fetch_print.c
 * `gcc fetch_print.c -lvapi -o fetch_print`
 */
#pragma once
#include <stdio.h>

/**
 * @brief \c send() HTTP Request over a TCP socket, wrapping the response socket over
 * the returned `FILE *` stream.
 *
 * Connects to host at URL over tcp and writes optional HTTP fields in INIT
 * to the request. It returns a readable FILE stream that separates the frames
 * by newlines, so you can read each logical frame one by one easier.
 *
 * INIT slots are:
 *  - [0]: Method case insensitive
 *  - [1]: Headers
 *  - [2]: Body
 *  - [3]: *plain int* Body parser frame type.
 *
 * INIT[3] is the only slot that #fetch will read as a plain
 * `uint64`.
 *
 * @retval NOT_0 OK - Anything not 0 means the response stream was successfully opened.
 * @retval NULL Error - Check `errno` to learn about the error (too many to list here).
 *
 * ### Typicode API Example
 * @snippet fetch_print.c fetch basic usage
 */
FILE *fetch(const char *url, const char *init[4]);
