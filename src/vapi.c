#include "lib/stream.h"
#include "lib/fetch.h"
#include <asm-generic/errno-base.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

FILE *fetch(const char *url, const char *init[4]) {
    int fds[4] = {0};
    struct dispatch *dispatch = fetch_socket(url, init);
    if (!dispatch) {
        return perror_rc(NULL, "fetch_socket()", 0);
    }
    char *hostname = strdup(dispatch->url.hostname.hd);
    int rc = use_fetch(fds, dispatch);
    if (rc != 0) {
        perror("use_fetch()");
        return NULL;
    }
    struct fetch_state *fs = calloc(1, sizeof(struct fetch_state));
    if (!fs) {
        perror("calloc()");
        return NULL;
    }

    fs->ssl = dispatch->ssl, fs->ssl_ctx = dispatch->ctx;
    fs->netfd = fds[0], fs->outfd = fds[2], fs->ep = fds[3];
    int appfd = fds[1];
    fs->headers_done = false;
    fs->header_len = 0;
    fs->hostname = hostname;

    // Initialize body parsing state
    fs->chunked_mode = false;
    fs->current_chunk_size = 0;
    fs->reading_chunk_size = true;
    fs->chunk_line_len = 0;

    fs->stream = cookie(&COOKIE_JSON);

    fs->http_done = false;

    // spawn background worker thread
    pthread_t tid = 0;
    pthread_create(&tid, NULL, fetcher, fs);
    pthread_detach(tid); // detach so it cleans up after finishing

    FILE *fetchfile = fdopen(appfd, "r");
    if (!fetchfile) {
        return perror_rc(NULL, "fdopen()", close(appfd));
    }
    dispatch_free(dispatch);
    return fetchfile;
}
