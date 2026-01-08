#include "pyc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

int min(int a, int b) {
    return a < b ? a : b;
}

int max(int a, int b) {
    return a > b ? a : b;
}

const struct str STR_EMPTY = {
    .hd = NULL,
    .length = 0
};

struct list {
    struct list *next;
    struct list *prev;
    struct str val;
};

struct queue {
    struct str *buffer;
    uint hd;
    uint size;
    size_t cap;
};

struct str str(const char *fmt, ...) {
    struct str out = {0};
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

    return (struct str) {
        .hd = p,
        .length = len
    };
}

struct str strn(char *hd, size_t length) {
    return (struct str) {.hd=hd, .length=length};
}

struct str __str_next(struct str s) {
    if (s.length == 0 || s.hd[0] == '\0') {
        return (struct str) {
            .hd = "",
            .length = 0
        };
    }

    return (struct str) {
        .hd = s.hd + 1,
        .length = s.length - 1 
    };
}

size_t __str_len(struct str s) {
    return s.length;
}

char *__str_get(struct str s) {
    return s.hd;
}

bool __str_done(struct str s) {
    int rc = 0;
    if (s.hd) {
        free(s.hd);
        rc += 1;
    }
    s.length = 0;
    return rc;
}

struct str __str_filter(struct str s, bool f (int c, uint i)) {
    if (!s.hd)
        return empty(struct str);

    uint p = 0;
    for (uint i = 0; i < s.length; i++) {
        if (f(s.hd[i], i))
            s.hd[p++] = s.hd[i];
    }
    s.hd[p] = '\0';
    s.length = p;

    return s;
}

bool __str_insert(struct str s, char ch) {
    if (!s.hd) {
        return false;
    }

    s.hd[s.length] = ch;
    s.hd[s.length + 1] = '\0';
    s.length += 1;
    return true;
}

struct str __str_map(struct str s, int f (int)) {
    if (!s.hd) {
        return empty(struct str);
    }
    struct str st = {
        .hd = s.hd,
        .length = s.length
    };
    for (uint i = 0; i < s.length; i++)
        st.hd[i] = f(s.hd[i]);
    return st;
}

struct str *__str_split(struct str s, struct str m, size_t *n) {
    if (!s.hd || !m.hd || !n) {
        return NULL;
    };
    *n = 0;

    const char *src = s.hd;
    size_t slen = s.length;

    const char *pat = m.hd;
    size_t plen = m.length;

    // Edge case: empty pattern -> return whole string as 1 token
    if (plen == 0) {
        struct str *arr = calloc(1, sizeof(*arr));
        if (!arr) return NULL;
        arr[0] = str(s.hd);
        *n = 1;
        return arr;
    }

    size_t count = 1;
    for (size_t i = 0; i + plen <= slen; ) {
        if (memcmp(src + i, pat, plen) == 0) {
            count++;
            i += plen;
        } else {
            i++;
        }
    }

    // token array
    struct str *parts = calloc(count, sizeof(*parts));
    if (!parts) return NULL;

    size_t idx = 0;
    size_t start = 0;

    for (size_t i = 0; i <= slen; ) {

        bool at_end = (i == slen);
        bool at_pat = false;

        if (!at_end && i + plen <= slen)
            at_pat = (memcmp(src + i, pat, plen) == 0);

        if (at_pat || at_end) {

            // token = substring [start, i)
            struct str token = __str_slice(s, start, i);

            if (!token.hd) {
                // cleanup all previous tokens
                for (size_t k = 0; k < idx; k++)
                    free(parts[k].hd);
                free(parts);
                *n = 0;
                return NULL;
            }

            parts[idx++] = token;

            if (at_pat) {
                i += plen;
                start = i;
                continue;
            }

            if (at_end)
                break;
        }

        i++;
    }

    *n = count;
    return parts;
}

struct str __str_slice(const struct str s, uint start, uint end) {
    if (!s.hd || start > end || end > s.length) {
        return empty(struct str);
    }
    size_t len = end - start;
    char *buf = malloc(len + 1);
    if (!buf)
        return empty(struct str);

    memcpy(buf, s.hd + start, len);
    buf[len] = '\0';
    return (struct str) {.hd=buf, .length=len};
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
        return empty(struct str);
    }
    return ls->val;
}

bool __list_done(struct list *ls) {
    if (!ls) {
        return false;
    }

    struct list *next_node = ls->next;
    struct list *prev_node = ls->prev;

    free(ls);

    while (next_node) {
        struct list *next_next = next_node->next;
        free(next_node);
        next_node = next_next;
    }

    while (prev_node) {
        struct list *prev_prev = prev_node->prev;
        free(prev_node);
        prev_node = prev_prev;
    }

    return true;
}

bool __list_insert(struct list *ls, struct str s) {
    if (!ls || !s.hd)
        return false;

    struct list *cur_tl = ls;
    while (cur_tl->next)
        cur_tl = cur_tl->next;

    struct list *new_tl = calloc(1, sizeof(struct list));

    new_tl->val = s;
    new_tl->prev = cur_tl;
    cur_tl->next = new_tl;
    return true;
}

struct list *__list_filter(struct list **ls, bool f (struct str s, uint i)) {
    if (!ls || !*ls || !f)
        return NULL;

    /* Find true head of input list */
    struct list *cur = *ls;
    while (cur->prev)
        cur = cur->prev;

    struct list *keep_head = NULL;
    struct list *keep_tail = NULL;
    struct list *rej_head  = NULL;
    struct list *rej_tail  = NULL;

    uint index = 0;

    while (cur) {
        struct list *next = cur->next;  // save traversal

        /* detach node completely */
        cur->prev = NULL;
        cur->next = NULL;

        if (f(cur->val, index++)) {
            /* append to kept list */
            if (!keep_head) {
                keep_head = keep_tail = cur;
            } else {
                keep_tail->next = cur;
                cur->prev = keep_tail;
                keep_tail = cur;
            }
        } else {
            /* append to rejected list */
            if (!rej_head) {
                rej_head = rej_tail = cur;
            } else {
                rej_tail->next = cur;
                cur->prev = rej_tail;
                rej_tail = cur;
            }
        }

        cur = next;
    }

    /* write rejected list head back to caller */
    *ls = rej_head;

    return keep_head;
}

struct str __queue_next(struct queue *q) {
    if (!q || q->size == 0) {
        return empty(struct str);
    }

    struct str popped = q->buffer[q->hd]; // pop
    q->hd += 1;
    q->size -= 1;
    return popped;
}

size_t __queue_len(struct queue *q) {
    if (!q) {
        fprintf(stderr, "Can't read len of a NULL queue\n");
        abort();
    }

    return q->size;
}

struct str __queue_get(struct queue *q) {
    if (!q) {
        fprintf(stderr, "Can't read front of a NULL queue\n");
        abort();
    }
    return q->buffer[q->hd];
}

bool __queue_done(struct queue *q) {
    if (!q) {
        return 0;
    }
    int rc = 1;

    if (q->buffer) {
        free(q->buffer);
        rc += 1;
    }
    free(q);

    return rc;
}

#define QUEUE_INIT_SIZE 8
bool __queue_insert(struct queue *q, struct str s) {
    if (!q || !s.hd) {
        return false;
    }
    if (!q->buffer) {
        // then we need to initialize the buffer
        q->buffer = calloc(QUEUE_INIT_SIZE, sizeof(struct str));
        q->cap = QUEUE_INIT_SIZE;
        q->size = 0;
    }

    /* Grow buffer if full */
    if (q->size == q->cap) {
        size_t oldcap = q->cap;
        size_t newcap = oldcap ? oldcap * 2 : 8;

        struct str *newbuf = calloc(newcap, sizeof(struct str));
        if (!newbuf) {
            return false;
        }

        /* Copy existing contents */
        for (size_t i = 0; i < q->size; i++) {
            size_t idx = (q->hd + i) % oldcap;
            newbuf[i] = q->buffer[idx];
        }

        free(q->buffer);
        q->buffer = newbuf;
        q->hd = 0;
        q->cap = newcap;
    }

    /* Insert at tail = (hd + size) % cap */
    size_t tl = (q->hd + q->size) % q->cap;
    q->buffer[tl] = s;
    q->size++;

    return true;
}
#undef QUEUE_INIT_SIZE

