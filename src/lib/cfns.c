#include "cfns.h"
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct string ss(const char *s, size_t len) {
    struct string str = {0};
    if (!s)
        return str;
    return (struct string) { .hd = (char *) s, .length = len };
}

struct string dynamic(const char *fmt, ...) {
    struct string out = {0};
    va_list ap, ap2;

    // --- First pass: measure ---
    va_start(ap, fmt);
    va_copy(ap2, ap);

    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0) {
        errno = EINVAL;
        va_end(ap2);
        return out;
    }

    size_t len = (size_t)needed;
    char *p = (char *) calloc(1, len + 1);
    if (!p) {
        va_end(ap2);
        return out;
    }

    vsnprintf(p, len + 1, fmt, ap2);
    va_end(ap2);
    return (struct string) {
        .hd = p,
        .length = len
    };
}

struct string slice(const struct string s, size_t start, size_t end) {
    struct string out = { .hd = NULL, .length = 0 };
    if (!s.hd || start > end || end > s.length) {
        return out;
    }
    struct string slice = {
        .hd=NULL, .length=0
    };
    size_t len = end - start;

    char *buf = malloc(len + 1);
    if (!buf) return out;

    memcpy(buf, s.hd + start, len);
    buf[len] = '\0';

    out.hd = buf;
    out.length = len;
    return out;
}


struct string stringdup(const struct string s) {
    return (struct string) {
        .hd = strndup(s.hd, s.length),
        .length = s.length
    };
}

struct string *split(const struct string s,
                     const struct string pattern,
                     size_t *ntokens)
{
}

struct string *splitch(const struct string s, char delim,
                       size_t *ntoks)
{
    if (!s.hd || !ntoks) {
        return NULL;
    }

    size_t count = 1;
    for (const char *p = s.hd; *p; p++) {
        if (*p == delim) count++;
    }

    struct string *tokens = calloc(count, sizeof(struct string));
    if (!tokens) return NULL;

    int index = 0;
    const char *start = s.hd;
    const char *p = s.hd;

    for (;;) {
        if (*p == delim || *p == '\0') {
            size_t token_length = p - start;
            if (token_length <= 0) {
                for (int i = 0; i < index; i++) {
                    free(tokens[i].hd);
                }
                free(tokens);
                return NULL;
            }
            char *token_buffer = calloc(token_length + 1, sizeof(char));
            if (!token_buffer) { // bail
                for (size_t i = 0; i < index; i++) free(tokens[i].hd);
                free(tokens);
                return NULL;
            }
            memcpy(token_buffer, start, token_length);
            token_buffer[token_length] = '\0';

            tokens[index].hd = token_buffer;
            tokens[index].length = token_length;
            index++;

            if (*p == '\0') break;
            start = p + 1;
        }
        p++;
    }
    *ntoks = index;
    return tokens;
}

// Sets S::HD to F(S::HD) in place, so it's like a map where
// the out buffer is the same as the in buffer.
static int rewrite(struct string *s, int (*f) (int)) {
    assert(s && s->hd && "S or S->hd can't be NULL");
    assert(f && "Map function F can't be NULL");
    for (int i = 0; i < s->length; i++) {
        s->hd[i] = f(s->hd[i]);
    }
    return 0;
}

int lowercase(struct string *s) {
    return rewrite(s, tolower);
}

int uppercase(struct string *s) {
    return rewrite(s, toupper);
}

int rmch(struct string *s, char ch) {
    if (!s || !s->hd) return 1;
    int w = 0;
    for (int r = 0; r < s->length; r++) {
        if (s->hd[r] != ch) {
            s->hd[w++] = s->hd[r];
        }
    }
    s->hd[w] = '\0';
    s->length = w;
    return 0;
}

struct string to_lowercase(const struct string s) {
    struct string out = {0};
    if (!s.hd) return out;

    out.length = s.length;
    out.hd = calloc(out.length + 1, sizeof(char));
    if (!out.hd) { // ENOMEM
        return out;
    }
    memcpy(out.hd, s.hd, out.length + 1);
    if (lowercase(&out) != 0) {
        free(out.hd);
        return out;
    }
    return out;
}

struct string uppercase_im(const struct string s) {
    assert(s.hd && "can't write out to a NULL S::HD");

    struct string out = {0};
    out.length = s.length;
    out.hd = calloc(out.length + 1, sizeof(char));
    if (!out.hd) { // ENOMEM
        return out;
    }
    memcpy(out.hd, s.hd, out.length + 1);
    if (uppercase(&out) != 0) {
        free(out.hd);
        return out;
    }
    return out;
}

void deque8_init(struct deque8 *deque) {
    deque->cap = 8;
    deque->buffer = calloc(deque->cap, sizeof(char *));
    deque->hd = deque->tl = deque->count = 0;
}

void deque8_push(struct deque8 *q, char *val) {
    if (q->count == q->cap) {
        size_t oldcap = q->cap;
        size_t newcap = oldcap * 2;

        char **newbuf = calloc(newcap, sizeof(char *));
        if (!newbuf) return;

        // copy linearized existing content into new buffer
        // in order (head ... oldcap-1, 0 ... head-1)
        for (size_t i = 0; i < q->count; i++) {
            size_t idx = (q->hd + i) % oldcap;
            newbuf[i] = q->buffer[idx];
        }

        free(q->buffer);
        q->buffer = newbuf;

        q->hd = 0;
        q->tl = q->count;
        q->cap  = newcap;
    }

    q->buffer[q->tl] = val;
    q->tl = (q->tl + 1) % q->cap;
    q->count++;
}

void deque8_free(struct deque8 *q) {
    if (!q || !q->buffer) return;

    for (size_t i = 0; i < q->count; i++) {
        size_t idx = (q->hd + i) % q->cap;

        if (q->buffer[idx])
            free(q->buffer[idx]);
    }

    free(q->buffer);
    free(q);
}

char *deque8_pop(struct deque8 *q) {
    if (q->count == 0)
        return NULL;

    char *val = q->buffer[q->hd];
    q->buffer[q->hd] = 0;
    q->hd = (q->hd + 1) % q->cap;
    q->count--;

    return val;
}

