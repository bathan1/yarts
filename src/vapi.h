/**
 * @file vapi.h
 * @brief Yet Another Runtime TCP Stream internal functions API
 *
 * Helper functions #yarts.c has available.
 *
 * @example stream_print.c
 * `gcc stream_print.c -lyarts -o stream_print`
 *
 * @example fetch_print.c
 * `gcc fetch_print.c -lyarts -o fetch_print`
 */
#pragma once
#include <stdio.h>

/**
 * @brief Make FILES[1] write to readable FILES[0].
 *
 * Opens a unidirectional pipe where you write into `FILES[0]` and 
 * read from `FILES[1]`, just like posix pipes but with in-memory FILE
 * handles.
 *
 * @retval  0  Success. `FILES[0]` and `FILES[1]` are fully initialized.
 * @retval -1  Error. `FILES` is left unchanged and errno is set.
 *
 * ### Example with manual JSON
 * @snippet stream_print.c Stream basic usage
 */
int stream(FILE *files[2]);

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
