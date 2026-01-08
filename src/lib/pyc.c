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

struct queue {
    struct str *buffer;
    uint hd;
    uint size;
    size_t cap;
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

bool __str_insert(struct str s, char ch) {
    if (!s.val) {
        return false;
    }

    s.val[s.length] = ch;
    s.val[s.length + 1] = '\0';
    s.length += 1;
    return true;
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

bool __list_insert(struct list *ls, struct str s) {
    if (!ls || !s.val) {
        return false;
    }

    struct list *cur_tl = ls;
    while (cur_tl->next) {
        cur_tl = cur_tl->next;
    }

    struct list *new_tl = calloc(1, sizeof(struct list));

    new_tl->val = s;
    new_tl->prev = cur_tl;
    cur_tl->next = new_tl;
    return true;
}

struct str __queue_next(struct queue *q) {
    if (!q || q->size == 0) {
        return empty(str);
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
    if (!q || !s.val) {
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

int main() {
    struct str fst = {.val="hello", .length=sizeof("hello") - 1};
    struct str sec = {.val=" ", .length=sizeof(" ") - 1};
    struct str lst = {.val="world!\n", .length=sizeof("world!\n") - 1};

    struct queue q = {
        .cap = 8,
        .buffer = calloc(8, sizeof(struct str)),
        .hd = 0,
        .size = 0
    };

    insert(&q, fst);
    insert(&q, sec);
    insert(&q, lst);

    while (len(&q) > 0) {
        get(&q);
        struct str popped = next(&q);
        printf("%s", popped.val);
    }

    return 0;
}

