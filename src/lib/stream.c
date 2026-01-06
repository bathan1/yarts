#include "cfns.h"

#include <errno.h>
#include <stdlib.h>
#define _GNU_SOURCE
#include <yyjson.h>
#include <yajl/yajl_parse.h>


/** Fixed number of JSON object levels to traverse before returning. */
#define MAX_DEPTH 64

#define peek(cur, field) (cur->field[cur->current_depth - 1])
#define push(cur, field, value) ((cur->field[cur->current_depth]) = value)

struct stream_cookie_writable {
    yajl_handle parser;
    unsigned int current_depth;
    unsigned int depth;
    struct deque8 *queue;

    // clarinet frees everything from here
    char **keys;
    size_t keys_size;
    size_t keys_cap;
    // key stack
    char *key[MAX_DEPTH];
    yyjson_mut_doc *doc_root;
    // object node stack
    yyjson_mut_val *object[MAX_DEPTH];

    unsigned int pp_flags;
};

struct stream_cookie_readable {
    struct deque8 *queue;
    char *current;
    size_t len;
    size_t offset;
    bool emit_newline;
};

/** YAJL parser callbacks */
static yajl_callbacks callbacks;
/** Close write end @ COOKIE. */
static ssize_t stream_fwrite(void *cookie, const char *buf, size_t size);
/** Close read end @ COOKIE. */
static ssize_t stream_fread(void *cookie, char *buf, size_t size);
static int stream_fclosew(void *cookie);
static int stream_fcloser(void *cookie);

/** Initialize bassoon state on the heap and get back that pointer. */
static struct stream_cookie_writable *use_state();
static void free_state(struct stream_cookie_writable *writable);

FILE *rstream(struct deque8 *init) {
    cookie_io_functions_t io = {
        .read  = stream_fread,
        .close = stream_fcloser,
        .write = NULL,
        .seek  = NULL,
    };

    struct stream_cookie_readable *readable = calloc(1, sizeof(struct stream_cookie_readable));
    readable->queue = init;
    return fopencookie(readable, "r", io);
}

FILE *wstream(struct deque8 *init) {
    struct stream_cookie_writable *writable = use_state();
    if (!writable) {
        perror("use_state");
        return NULL;
    }
    writable->queue = init;
    writable->parser = yajl_alloc(&callbacks, NULL, (void *) writable);
    if (!writable->parser) {
        perror("yajl_alloc");
        free(writable);
        return NULL;
    }

    cookie_io_functions_t io = {
        .write = stream_fwrite,
        .close = stream_fclosew,
        .read  = NULL,
        .seek  = NULL,
    };

    return fopencookie(writable, "w", io);
}

/* BEGIN STATIC */
static ssize_t stream_fwrite(void *cookie, const char *buf, size_t size) {
    struct stream_cookie_writable *c = cookie;
    yajl_parse(c->parser, (const unsigned char *)buf, size);
    return size;
}

static int stream_fclosew(void *cookie) {
    struct stream_cookie_writable *cc = (void *) cookie;
    if (!cc) {
        return 1;
    }

    if (cc->parser) {
        yajl_free(cc->parser);
    }
    if (cc->keys) {
        free(cc->keys);
    }
    free(cc);

    return 0;
}

static ssize_t stream_fread(void *cookie, char *buf, size_t size)
{
    struct stream_cookie_readable *c = cookie;

    if (size == 0)
        return 0;

    size_t out = 0;

    while (out < size) {

        /* Emit newline if pending */
        if (c->emit_newline) {
            buf[out++] = '\n';
            c->emit_newline = false;
            return out;   // return immediately (stream semantics)
        }

        /* Load next JSON object if needed */
        if (!c->current) {
            c->current = deque8_pop(c->queue);
            if (!c->current)
                return out;  // EOF if nothing written

            c->len = strlen(c->current);
            c->offset = 0;
        }

        /* Emit JSON bytes */
        size_t remaining = c->len - c->offset;
        size_t to_copy = remaining < (size - out)
            ? remaining
            : (size - out);

        memcpy(buf + out, c->current + c->offset, to_copy);

        c->offset += to_copy;
        out += to_copy;

        /* Finished this object */
        if (c->offset == c->len) {
            free(c->current);
            c->current = NULL;
            c->emit_newline = true;  // <-- CRITICAL
        }

        /* Return once we’ve produced something */
        if (out > 0)
            return out;
    }

    return out;
}

static int stream_fcloser(void *cookie) {
    struct stream_cookie_readable *state = cookie;
    struct deque8 *queue = (void *) state->queue;
    if (!queue) { return 1; }
    deque8_free(queue);
    free(state);
    return 0;
}

static int handle_null(void *ctx) {
    struct stream_cookie_writable *state = ctx;
    if (state->current_depth == 0) {
        fprintf(stderr, "current_depth is 0\n");
        return 0;
    }
    if (!peek(state, key)) {
        fprintf(stderr, "no parent key value from depth %u\n", state->current_depth);
        return 0;
    }
    yyjson_mut_obj_add_null(state->doc_root, peek(state, object), peek(state, key));
    return 1;
}

static int handle_bool(void *ctx, int b) {
    struct stream_cookie_writable *state = ctx;
    if (state->current_depth == 0) {
        fprintf(stderr, "current_depth is 0\n");
        return 0;
    }
    if (!peek(state, key)) {
        fprintf(stderr, "no parent key value from depth %u\n", state->current_depth);
        return 0;
    }
    yyjson_mut_obj_add_bool(
        state->doc_root,
        peek(state, object),
        peek(state, key),
        b
    );
    return 1;
}

static int handle_int(void *ctx, long long i) {
    return 1;
}

static int handle_double(void *ctx, double d) {
    return 1;
}

static int handle_number(void *ctx, const char *num, size_t len) {
    struct stream_cookie_writable *cur = ctx;
    if (cur->current_depth == 0) {
        fprintf(stderr, "current_depth is 0\n");
        return 0;
    }
    if (!peek(cur, key)) {
        fprintf(stderr, "no parent key value from depth %u\n", cur->current_depth);
        return 0;
    }

    bool is_float = false;
    for (size_t i = 0; i < len; i++) {
        char c = num[i];
        if (c == '.' || c == 'e' || c == 'E') {
            is_float = true;
            break;
        }
    }

    if (is_float) {
        double d = strtod(num, NULL);
        yyjson_mut_obj_add_double(
            cur->doc_root,
            peek(cur, object),
            peek(cur, key),
            d
        );
    } else {
        long i = strtoll(num, NULL, 10);
        yyjson_mut_obj_add_int(
            cur->doc_root,
            peek(cur, object),
            peek(cur, key),
            i
        );
    }

    return 1;
}

static int handle_string(void *ctx,
                         const unsigned char *str, size_t len)
{
    struct stream_cookie_writable *cur = ctx;

    if (cur->current_depth == 0 || !peek(cur, key) || !peek(cur, object)) {
        return 0;
    }

    yyjson_mut_obj_add_strncpy(
        cur->doc_root,
        peek(cur, object),
        peek(cur, key),
        (char *) str,
        len
    );

    return 1;
}

static int handle_start_map(void *ctx) {
    struct stream_cookie_writable *cur = ctx;
    if (cur->current_depth == 0) {
        yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
        yyjson_mut_val *obj = yyjson_mut_obj(doc);
        yyjson_mut_doc_set_root(doc, obj);
        cur->doc_root = doc;
        cur->object[0] = yyjson_mut_doc_get_root(doc);
    } else {
        yyjson_mut_val *new_obj = yyjson_mut_obj(cur->doc_root);
        yyjson_mut_obj_add_val(
            cur->doc_root,
            peek(cur, object),
            peek(cur, key),
            new_obj
        );
        cur->object[cur->current_depth] = new_obj;
    }

    cur->current_depth++;

    return 1;
}


static int handle_map_key(void *ctx,
                          const unsigned char *str,
                          size_t len)
{
    struct stream_cookie_writable *cur = ctx;
    if (cur->keys_size >= cur->keys_cap) {
        // double
        cur->keys_cap *= 2;
        cur->keys = realloc(cur->keys, cur->keys_cap * sizeof(char *));
    }

    char *next_key = strndup((const char *) str, len);
    cur->keys[cur->keys_size++] = next_key;
    // Store the new key for this depth
    peek(cur, key) = next_key;

    return 1;
}

static int handle_end_map(void *ctx) {
    struct stream_cookie_writable *cur = ctx;
    if (cur->current_depth == 1) {
        // closing root object because root object set depth to 1,
        // so that any nested object child can recursively push its own
        // node to the key / parent stack

        yyjson_doc *final =
            yyjson_mut_doc_imut_copy(cur->doc_root, NULL);
        if (!final) {
            fprintf(stderr, "could not copy to immutable doc\n");
            return 0;
        }
        yyjson_mut_doc_free(cur->doc_root);

        if (cur->keys) {
            for (int i = 0; i < cur->keys_size; i++) {
                if (cur->keys[i])
                    free(cur->keys[i]);
            }
        }
        cur->keys_size = 0;

        char *json = yyjson_write(final, cur->pp_flags, NULL);
        // we push to queue
        deque8_push(cur->queue, json);

        // free(cur->queue.handle);
        yyjson_doc_free(final);
    }
    cur->current_depth--;
    cur->depth = MAX(cur->current_depth, cur->depth);

    return 1;
}

static int handle_start_array(void *ctx) {
    return 1;
}

static int handle_end_array(void *ctx) {
    return 1;
}

static yajl_callbacks callbacks = {
    .yajl_null        = handle_null,
    .yajl_boolean     = handle_bool,
    .yajl_integer     = handle_int,
    .yajl_double      = handle_double,
    .yajl_number      = handle_number,
    .yajl_string      = handle_string,

    .yajl_start_map   = handle_start_map,
    .yajl_map_key     = handle_map_key,
    .yajl_end_map     = handle_end_map,

    .yajl_start_array = handle_start_array,
    .yajl_end_array   = handle_end_array
};

static struct stream_cookie_writable *use_state(void) {
    struct stream_cookie_writable *st = calloc(1, sizeof(struct stream_cookie_writable));
    if (!st) return perror_rc(NULL, "calloc()", 0);

    st->keys_cap = 1 << 8;     // 256
    st->keys = calloc(st->keys_cap, sizeof(char *));
    if (!st->keys) {
        return perror_rc(NULL, "calloc()", free(st));
    }

    // IMPORTANT: do NOT allocate st->queue here.
    // It must be set by stream_writable() to point to caller's queue.
    st->queue = NULL;

    return st;
}

static void free_state(struct stream_cookie_writable *st) {
    if (!st) return;

    // Don't free st->queue here — it's not owned by the state!

    if (st->keys)
        free(st->keys);

    free(st);
}

size_t fwrite8(const char *src, size_t n,
               size_t max, FILE *dst)
{
    size_t to_copy = n < max ? n : max;
    size_t written = fwrite(src, sizeof(char), to_copy, dst);
    if (written == 0) {
        int err = errno;
        if (ferror(dst)) {
            fprintf(
                stderr,
                "[fwrite8] write failed (requested %zu bytes)",
                to_copy
            );
        }

        if (err != 0) {
            char *errmsg = strerror(err);
            fprintf(stderr, ": %s", errmsg);
            free(errmsg);
        }
        fprintf(stderr, "\n");
    }
    return written;
}

#undef push
#undef peek
#undef MAX_DEPTH
