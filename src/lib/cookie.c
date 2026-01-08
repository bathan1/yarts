#include "debug.h"
#include "pyc.h"

#include <errno.h>
#include <stdlib.h>
#define _GNU_SOURCE
#include <yyjson.h>
#include <yajl/yajl_parse.h>

/** Fixed number of JSON object levels to traverse before returning. */
#define MAX_DEPTH 64

#define push(cur, field, value) ((cur->field[cur->current_depth]) = value)

struct cookie {
    const cookie_io_functions_t f;

    void *(*make)(void *);        // allocate backend state
    void  (*destroy)(void *);     // free backend state
};

struct passthrough {
    struct queue *queue;
};

static ssize_t passthrough_write(void *c, const char *buf, size_t n) {
    struct passthrough *st = c;
    char *copy = malloc(n);
    memcpy(copy, buf, n);
    struct str copy_str = {.hd=copy, .length=n};
    insert(st->queue, copy_str);
    return n;
}

static ssize_t passthrough_read(void *c, char *buf, size_t n) {
    struct passthrough *st = c;
    struct str front = next(st->queue);
    if (!front.hd)
        return 0;
    size_t out = len(front) < n ? len(front) : n;
    memcpy(buf, front.hd, out);
    free(front.hd);
    return out;
}

static int passthrough_close(void *c) {
    struct passthrough *st = c;
    if (st->queue)
        done(st->queue);
    free(st);
    return 0;
}

static void *passthrough_make(void *_) {
    struct passthrough *st = calloc(1, sizeof *st);
    if (!st)
        return NULL;

    st->queue = calloc(1, sizeof(struct queue *));
    if (!st->queue) {
        free(st);
        return NULL;
    }

    return st;
}

static void passthrough_destroy(void *state) {
    struct passthrough *st = state;
    if (!st)
        return;

    if (st->queue)
        done(st->queue);

    free(st);
}

const struct cookie COOKIE_PASSTHROUGH = {
    .f = {
        .write = passthrough_write,
        .read  = passthrough_read,
        .close = passthrough_close,
        .seek  = NULL,
    },
    .make = passthrough_make,
    .destroy = passthrough_destroy,
};

struct json_writable {
    yajl_handle parser;
    struct list *path;
    struct list *path_parent;
    unsigned int current_depth;
    struct queue *queue;

    // JSON property names memory
    char **keys;
    size_t keys_size;
    size_t keys_cap;
    char *key_stack[MAX_DEPTH];

    yyjson_mut_doc *doc_root;
    yyjson_mut_val *object_stack[MAX_DEPTH];
    unsigned int pp_flags;
};

struct json_readable {
    struct queue *queue;
    char *current;
    size_t length;
    size_t offset;
    bool emit_newline;
};

typedef struct json {
    struct json_writable writable;
    struct json_readable readable;
} json_t;

static int handle_null(void *ctx) {
    struct json_writable *cur = ctx;
    if (cur->current_depth == 0) {
        fprintf(stderr, "current_depth is 0\n");
        return 0;
    }
    if (!(cur->key_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0])) {
        fprintf(stderr, "no parent key value from depth %u\n", cur->current_depth);
        return 0;
    }
    yyjson_mut_obj_add_null(
        cur->doc_root,
        cur->object_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0],
        cur->key_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0]
    );
    return 1;
}

static int handle_bool(void *ctx, int b) {
    struct json_writable *cur = ctx;
    if (cur->current_depth == 0) {
        fprintf(stderr, "current_depth is 0\n");
        return 0;
    }
    if (!cur->key_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0]) {
        fprintf(stderr, "no parent key value from depth %u\n", cur->current_depth);
        return 0;
    }
    yyjson_mut_obj_add_bool(
        cur->doc_root,
        cur->object_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0],
        cur->key_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0],
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
    struct json_writable *cur = ctx;
    if (cur->current_depth == 0) {
        fprintf(stderr, "current_depth is 0\n");
        return 0;
    }
    if (!cur->key_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0]) {
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
            cur->object_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0],
            cur->key_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0],
            d
        );
    } else {
        long i = strtoll(num, NULL, 10);
        yyjson_mut_obj_add_int(
            cur->doc_root,
            cur->object_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0],
            cur->key_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0],
            i
        );
    }

    return 1;
}

static int handle_string(void *ctx, const unsigned char *str, 
                         size_t len)
{
    struct json_writable *cur = ctx;
    if (cur->path) {
        return 1;
    }

    if (cur->current_depth == 0 
        || !(cur->object_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0])
        || !(cur->key_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0]))
    {
        return 0;
    }

    yyjson_mut_obj_add_strncpy(
        cur->doc_root,
        cur->object_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0],
        cur->key_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0],
        (char *) str,
        len
    );

    return 1;
}

static int handle_start_map(void *ctx) {
    struct json_writable *cur = ctx;
    if (!cur->path) {
        if (cur->current_depth == 0) {
            yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
            yyjson_mut_val *obj = yyjson_mut_obj(doc);
            yyjson_mut_doc_set_root(doc, obj);
            cur->doc_root = doc;
            cur->object_stack[0] = yyjson_mut_doc_get_root(doc);
        } else {
            yyjson_mut_val *new_obj = yyjson_mut_obj(cur->doc_root);
            yyjson_mut_obj_add_val(
                cur->doc_root,
                cur->object_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0],
                cur->key_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0],
                new_obj
            );
            cur->object_stack[cur->current_depth] = new_obj;
        }
        cur->current_depth++;
    }


    return 1;
}


static int handle_map_key(void *ctx,
                          const unsigned char *str,
                          size_t length)
{
    struct json_writable *cur = ctx;
    char *next_key = strndup((const char *) str, length);
    if (cur->path 
        && length == get(cur->path).length
        && strncmp(next_key, get(cur->path).hd, length) == 0)
    {
        if (next(cur->path) == NULL && cur->path_parent == NULL) {
            // this will only run once since path_parent is the same for every row
            cur->path_parent = cur->path;
        }
        cur->path = next(cur->path);
    }

    if (cur->path) {
        return 1;
    }

    if (cur->keys_size >= cur->keys_cap) {
        // double
        cur->keys_cap *= 2;
        cur->keys = realloc(cur->keys, cur->keys_cap * sizeof(char *));
    }

    cur->keys[cur->keys_size++] = next_key;
    // Store the new key for this depth
    cur->key_stack[cur->current_depth > 0 ? cur->current_depth - 1 : 0] = next_key;

    return 1;
}

static int handle_end_map(void *ctx) {
    struct json_writable *cur = ctx;
    if (cur->path) {return 1;}

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
        struct str json_str = {.hd=json,.length=strlen(json)};
        // we push to queue
        insert(cur->queue, json_str);

        yyjson_doc_free(final);
        cur->path = cur->path_parent;
    }
    cur->current_depth--;

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

static struct json_writable *use_state(void) {
    struct json_writable *st = calloc(1, sizeof(struct json_writable));
    if (!st)
        return enomem(NULL);

    st->keys_cap = 1 << 8;     // 256
    st->keys = calloc(st->keys_cap, sizeof(char *));
    if (!st->keys) {
        free(st);
        return enomem(NULL);
    }

    // IMPORTANT: do NOT allocate st->queue here.
    // It must be set by stream_writable() to point to caller's queue.
    st->queue = NULL;
    return st;
}

size_t fwrite8(const char *src, size_t n, FILE *dst)
{
    size_t written = fwrite(src, sizeof(char), n, dst);
    if (written == 0) {
        int err = errno;
        if (ferror(dst)) {
            fprintf(
                stderr,
                "[fwrite8] write failed (requested %zu bytes)",
                n
            );
        }

        if (err != 0) {
            char *errmsg = strerror(err);
            fprintf(stderr, ": %s", errmsg);
        }
        fprintf(stderr, "\n");
    }
    return written;
}

static ssize_t json_fwrite(void *__cookie, const char *buf, size_t size) {
    json_t *cookie = __cookie;
    yajl_parse(cookie->writable.parser, (const unsigned char *)buf, size);
    return size;
}

static int json_fclose(void *__cookie) {
    int rc = 0;
    json_t *cookie = (void *) __cookie;
    if (!cookie) {
        rc += 1;
    }

    // cleanup write end
    if (cookie->writable.parser) {
        yajl_free(cookie->writable.parser);
    }
    if (cookie->writable.keys) {
        free(cookie->writable.keys);
    }

    /// cleanup queue
    if (!cookie->readable.queue) { 
        rc += 1;
    }
    done(cookie->readable.queue);

    free(cookie);
    return 0;
}

static ssize_t json_fread(void *__cookie, char *buf, size_t size)
{
    json_t *cookie = __cookie;

    if (size == 0)
        return 0;

    size_t out = 0;

    while (out < size) {
        /* Emit newline if pending */
        if (cookie->readable.emit_newline) {
            buf[out++] = '\n';
            cookie->readable.emit_newline = false;
            return out;   // return immediately (stream semantics)
        }

        /* Load next JSON object if needed */
        if (!cookie->readable.current) {
            struct str front = next(cookie->readable.queue);
            if (!front.hd)
                return out;
            cookie->readable.current = front.hd;

            cookie->readable.length = len(front);
            cookie->readable.offset = 0;
        }

        /* Emit JSON bytes */
        size_t remaining = cookie->readable.length - cookie->readable.offset;
        size_t to_copy = remaining < (size - out)
            ? remaining
            : (size - out);

        memcpy(buf + out, cookie->readable.current + cookie->readable.offset, to_copy);

        cookie->readable.offset += to_copy;
        out += to_copy;

        if (cookie->readable.offset == cookie->readable.length) {
            if (cookie->readable.current)
                free(cookie->readable.current);

            cookie->readable.current = NULL;
            cookie->readable.emit_newline = true;
        }

        /* Return once we’ve produced something */
        if (out > 0)
            return out;
    }

    return out;
}

static void *json_make(void *__path) {
    struct json *jc = calloc(1, sizeof *jc);
    if (!jc)
        return NULL;

    /* keys */
    jc->writable.keys_cap = 256;
    jc->writable.keys = calloc(jc->writable.keys_cap, sizeof(char *));
    if (!jc->writable.keys)
        goto fail;

    /* queue */
    jc->writable.queue = jc->readable.queue =
        calloc(1, sizeof(struct queue *));
    if (!jc->readable.queue)
        goto fail;

    /* body path */
    jc->writable.path = __path;
    jc->writable.path_parent = NULL;

    /* yajl parser */
    jc->writable.parser =
        yajl_alloc(&callbacks, NULL, &jc->writable);
    if (!jc->writable.parser)
        goto fail;

    return jc;

fail:
    if (jc->writable.parser) yajl_free(jc->writable.parser);
    if (jc->readable.queue) done(jc->readable.queue);
    free(jc->writable.keys);
    free(jc);
    return NULL;
}

static void json_destroy(void *state) {
    struct json *jc = state;
    if (!jc)
        return;

    /* yajl */
    if (jc->writable.parser)
        yajl_free(jc->writable.parser);

    /* keys */
    if (jc->writable.keys) {
        for (size_t i = 0; i < jc->writable.keys_size; i++)
            free(jc->writable.keys[i]);
        free(jc->writable.keys);
    }

    /* queue */
    if (jc->readable.queue)
        done(jc->readable.queue);

    free(jc);
}

const struct cookie COOKIE_JSON = {
    .f = {
        .write = json_fwrite,
        .close = json_fclose,
        .read  = json_fread,
        .seek  = NULL,
    },
    .make = json_make,
    .destroy = json_destroy
};

FILE *cookie(const struct cookie *cfns, void *ctx) {
    if (!cfns || !cfns->make)
        return NULL;

    void *state = cfns->make(ctx);
    if (!state)
        return NULL;

    FILE *f = fopencookie(state, "w+", cfns->f);
    if (!f) {
        /* fopencookie failed — backend state must be destroyed */
        if (cfns->destroy)
            cfns->destroy(state);
        return NULL;
    }

    /* Disable stdio buffering — backend controls buffering */
    setvbuf(f, NULL, _IONBF, 0);

    return f;
}

#undef push
#undef MAX_DEPTH
