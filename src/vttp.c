// Copyright 2025 Nathanael Oh. All Rights Reserved.
#include "lib/cookie.h"
#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include "vapi.h"
#include "lib/sql.h"

// uncomment to remove all debug prints
#define NDEBUG
#include <assert.h>
#include <asm-generic/errno.h>
#include <unistd.h>
#include <openssl/types.h>
#include <yyjson.h>
#include <curl/curl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

yyjson_doc *next_json_obj(FILE *stream, char **errmsg) {
    char *buf = NULL;
    size_t cap = 0;

    ssize_t n = getline(&buf, &cap, stream);
    if (n == -1) {
        if (errmsg) {
            *errmsg = sqlite3_mprintf("(vttp) no body stream");
        }
        free(buf);
        return NULL;
    }

    /* Trim trailing newline ONLY — standard for NDJSON */
    if (n > 0 && buf[n - 1] == '\n') {
        buf[n - 1] = '\0';
        n -= 1;
    }

    /* Parse */
    yyjson_doc *doc = yyjson_read(buf, n, 0);
    if (!doc) {
        if (errmsg) {
            *errmsg = sqlite3_mprintf("(vttp) invalid json object");
        }
        free(buf);
        return NULL;
    }

    free(buf);
    return doc;
}

/**
 * The SQLite virtual table
 */
typedef struct {
    /**
     * For SQLite to fill in
     */
    sqlite3_vtab base;


    /**
     * The #column_defs for this row
     */
    struct column_def *column_defs;

    /** Number of COLUMNS_DEFS in the allocated buffer. */
    size_t column_defs_count;

    uint icol_to_arg_index[4];
} vttp_vtab;

/// Cursor
typedef struct vttp_cursor {
    sqlite3_vtab_cursor base;
    FILE *stream;
    unsigned int count;
    int eof;

    // Completed row (a fully constructed immutable doc)
    yyjson_doc *next_doc;
} vttp_cursor_t;

#define X_UPDATE_OFFSET 2

// For tokens "vttp" (module name), "main" (schema), "patients" (vtable name), and
// at least 1 argument for the url argument
#define MIN_ARGC 4

static vttp_vtab *vttp_vtab_init(sqlite3 *db, int argc,
                          const char *const *argv, char **schema)
{
    vttp_vtab *vtab = sqlite3_malloc(sizeof(vttp_vtab));
    if (!vtab) {
        fprintf(stderr, "sqlite3_malloc() out of memory\n");
        return NULL;
    }
    memset(vtab, 0, sizeof(vttp_vtab));

    // DELETEME
    vtab->column_defs = parse_column_defs(argc, argv, &vtab->column_defs_count);

    /* max number of tokens valid inside a single xCreate argument for the table declaration */
    struct str first_line = str("create table %s(", argv[2]);
    sqlite3_str *s = sqlite3_str_new(db);
    sqlite3_str_appendall(s, first_line.hd);

    for (int i = 0; i < vtab->column_defs_count; i++) {
        sqlite3_str_appendf(
            s,
            "%s %s",
            vtab->column_defs[i].name.hd,
            vtab->column_defs[i].typename.hd
        );

        if (i < 3) {
            // hidden column_defs come first in declaration
            sqlite3_str_appendf(s, " %s", "hidden");
        }

        if (i + 1 < vtab->column_defs_count)
            sqlite3_str_appendall(s, ",");
    }
    sqlite3_str_appendall(s, ")");
    if (schema) {
        *schema = sqlite3_str_finish(s);
    }
    free(first_line.hd);
    return vtab;
}

static int vttpConnect(sqlite3 *pdb, void *paux, int argc,
                     const char *const *argv, sqlite3_vtab **pp_vtab,
                     char **pz_err)
{
    if (argc < MIN_ARGC) {
        fprintf(stderr, "Expected %d args but got %d args\n", MIN_ARGC, argc);
        return SQLITE_ERROR;
    }
    int rc = SQLITE_OK;
    char *schema = NULL;
    *pp_vtab = (sqlite3_vtab *) vttp_vtab_init(pdb, argc, argv, &schema);
    vttp_vtab *vtab = (vttp_vtab *) *pp_vtab;
    if (!vtab) {
        return SQLITE_NOMEM;
    }

    rc += sqlite3_declare_vtab(pdb, schema);

    sqlite3_free(schema);
    return rc;
}

/**
 * Same implementation as xConnect, we just have to point to different fns so this isn't 
 * eponymous (can't be called as its own table e.g. SELECT * FROM fetch).
 */
static int vttpCreate(sqlite3 *pdb, void *paux, int argc,
                     const char *const *argv, sqlite3_vtab **pp_vtab,
                     char **pz_err)
{
    return vttpConnect(pdb, paux, argc, argv, pp_vtab, pz_err);
}

static bool is_usable_eq_cst(struct sqlite3_index_constraint *cst, uint index) {
    return (
        cst->iColumn == index // column index
        && cst->op == // was the operator '='?
            SQLITE_INDEX_CONSTRAINT_EQ
        && cst->usable
    );
}

/* expected bitmask value of the index_info from xBestIndex */
#define REQUIRED_BITS 0b01

/** Check INDEX_INFO value against REQUIRED_BITS mask. */
static int check_plan_mask(struct sqlite3_index_info *index_info,
                           sqlite3_vtab *pVtab)
{
    vttp_vtab *vtab = (void *) pVtab;
    if ((index_info->idxNum & REQUIRED_BITS) == REQUIRED_BITS
        || (vtab->column_defs[ICOL_URL].default_value.length > 0))
        return SQLITE_OK; // Then either a `where url = ...` or a default value was set (or both)

    bool is_url_eq_cst = false;

    for (int i = 0; i < index_info->nConstraint; i++) {
        struct sqlite3_index_constraint *cst = &index_info->aConstraint[i];

        if (cst->iColumn == ICOL_URL) {
            is_url_eq_cst = true; // User passed "url" constraint but it somehow failed...
            if (!cst->usable)
                return SQLITE_CONSTRAINT;
        }
    }

    if (!is_url_eq_cst) {
        if (vtab->column_defs[ICOL_URL].default_value.length < 1) {
            pVtab->zErrMsg = sqlite3_mprintf(
                "(vttp) Missing `WHERE url = ...` (no default URL)."
            );
            return SQLITE_MISUSE;
        }
    }

    return SQLITE_ERROR;
}
#undef REQUIRED_BITS

/**
 * Fetch vtab's sqlite_module->xBestIndex() callback
 */
static int vttpBestIndex(sqlite3_vtab *pVTab, sqlite3_index_info *pIdxInfo) {
    int argPos = 1;
    int planMask = 0;
    vttp_vtab *vtab = (vttp_vtab *)pVTab;

    for (int i = 0; i < pIdxInfo->nConstraint; i++) {
        struct sqlite3_index_constraint *cst = &pIdxInfo->aConstraint[i];

        if (!cst->usable || cst->op != SQLITE_INDEX_CONSTRAINT_EQ)
            continue;

        struct sqlite3_index_constraint_usage *usage =
            &pIdxInfo->aConstraintUsage[i];

        if (is_usable_eq_cst(cst, ICOL_URL)) {
            vtab->icol_to_arg_index[ICOL_URL] = argPos - 1;
            usage->omit = 1;
            usage->argvIndex = argPos++;
            planMask |= ICOL_BIT(ICOL_URL);
        } 

        if (is_usable_eq_cst(cst, ICOL_BODY)) {
            vtab->icol_to_arg_index[ICOL_BODY] = argPos - 1;
            usage->omit = 1;
            usage->argvIndex = argPos++;
            planMask |= ICOL_BIT(ICOL_BODY);
        } 
    }

    pIdxInfo->idxNum = planMask;
    return check_plan_mask(pIdxInfo, pVTab);
}

/**
 * Cleanup virtual table state pointed to be P_VTAB.
 * Serves as both xDestroy and xDisconnect for the vtable.
 */
static int vttpDisconnect(sqlite3_vtab *pvtab) {
    vttp_vtab *vtab = (vttp_vtab *) pvtab;

    for (uint i = 3; i < vtab->column_defs_count; i++) {
        done(vtab->column_defs[i].default_value);
        done(vtab->column_defs[i].name);
        done(vtab->column_defs[i].typename);
    }
    free(vtab->column_defs);
    vtab->column_defs = 0;
    vtab->column_defs_count = 0;

    sqlite3_free(pvtab);
    return SQLITE_OK;
}

/**
 * Initialize fetch cursor at P_VTAB's cursor PP_CURSOR with count = 0.
 */
static int vttpOpen(sqlite3_vtab *pvtab, sqlite3_vtab_cursor **pp_cursor) {
    vttp_vtab *fetch = (vttp_vtab *) pvtab;
    vttp_cursor_t *cur = sqlite3_malloc(sizeof(vttp_cursor_t));
    if (!cur) {
        return SQLITE_NOMEM;
    }
    memset(cur, 0, sizeof(vttp_cursor_t));

    cur->count = 0;

    *pp_cursor = (sqlite3_vtab_cursor *)cur;
    (*pp_cursor)->pVtab = pvtab;
    return SQLITE_OK;
}

static int vttpClose(sqlite3_vtab_cursor *cur) {
    vttp_cursor_t *cursor = (vttp_cursor_t *)cur;
    if (cursor) {
        if (cursor->next_doc) {
            yyjson_doc_free(cursor->next_doc);
        }
        if (cursor->stream) {
            fclose(cursor->stream);
        }
        sqlite3_free(cur);
    }
    return SQLITE_OK;
}

static int vttpNext(sqlite3_vtab_cursor *cur0) {
    vttp_cursor_t *cur = (vttp_cursor_t*)cur0;
    vttp_vtab *vtab = (void*) cur->base.pVtab;


    // Sanity: next_doc must always contain the row returned previously.
    if (!cur->next_doc) {
        vtab->base.zErrMsg = sqlite3_mprintf(
            "unexpected NULL next_doc in cursor"
        );
        return SQLITE_ERROR;
    }

    yyjson_doc *prev = cur->next_doc;
    char *errmsg = NULL;
    cur->next_doc = next_json_obj(cur->stream, &errmsg);
    yyjson_doc_free(prev);

    return SQLITE_OK;
}

static void json_bool_result(
    sqlite3_context *pctx,
    struct column_def *def,
    yyjson_val *column_val
) {
    if (strncmp(hd(def->typename), "int", 3) == 0 
        || strncmp(hd(def->typename), "float", 5) == 0)
        sqlite3_result_int(pctx, yyjson_get_bool(column_val));
    else
        sqlite3_result_text(pctx, yyjson_get_bool(column_val) ? "true" : "false", -1, SQLITE_TRANSIENT);
}

static yyjson_val *follow_generated_path(
    yyjson_val *root,
    struct str *keys,
    size_t count
) {
    if (count == 0)
        return NULL;
    yyjson_val *cur = root;

    for (size_t i = 0; i < count; i++) {
        if (!cur || yyjson_get_type(cur) != YYJSON_TYPE_OBJ)
            return NULL;

        /* Pass exact byte count to yyjson */
        cur = yyjson_obj_getn(cur, keys[i].hd, keys[i].length);
        char *json = yyjson_val_write(cur, YYJSON_WRITE_PRETTY, NULL);
    }

    return cur;
}

/** Populates the Fetch row */
static int xColumn(sqlite3_vtab_cursor *pcursor,
                    sqlite3_context *pctx,
                    int icol)
{
    vttp_cursor_t *cursor = (vttp_cursor_t *)pcursor;
    vttp_vtab *vtab = (void *) cursor->base.pVtab;

    if (!cursor->next_doc) {
        fprintf(stderr, "expected a JSON pointer in next_doc but got 0\n");
        return SQLITE_ERROR;
    }

    if (icol < 3) // Skip hidden column_defs
        return SQLITE_OK;

    struct column_def def = vtab->column_defs[icol];

    yyjson_val *val = yyjson_doc_get_root(cursor->next_doc);

    char *json = yyjson_val_write(val, YYJSON_WRITE_PRETTY, NULL);

    if (def.generated_always_as_len > 0) {
        val = follow_generated_path(
            val,
            def.generated_always_as,
            def.generated_always_as_len
        );
    } else {
        val = yyjson_obj_getn(val, def.name.hd, def.name.length);
    }


    if (!val) {
        sqlite3_result_null(pctx);
        return SQLITE_OK;
    }

    switch (yyjson_get_type(val)) {
    case YYJSON_TYPE_STR:
        sqlite3_result_text(pctx, yyjson_get_str(val), -1, SQLITE_TRANSIENT);
        break;

    case YYJSON_TYPE_NUM:
        sqlite3_result_int(pctx, yyjson_get_int(val));
        break;

    case YYJSON_TYPE_BOOL:
        json_bool_result(pctx, &def, val);
        break;

    case YYJSON_TYPE_OBJ:
    case YYJSON_TYPE_ARR: {
        char *json = yyjson_val_write(val, 0, NULL);
        if (json) {
            sqlite3_result_text(pctx, json, -1, SQLITE_TRANSIENT);
            free(json);
        } else {
            sqlite3_result_null(pctx);
        }
        break;
    }

    default:
        sqlite3_result_null(pctx);
    }

    return SQLITE_OK;
}


static int xEof(sqlite3_vtab_cursor *cur) {
    vttp_cursor_t *c = (vttp_cursor_t*)cur;
    int rc = c->next_doc == NULL;
    return rc;
}

static int xRowid(sqlite3_vtab_cursor *pcursor, sqlite3_int64 *prowid) {
    *prowid = ((vttp_cursor_t *)pcursor)->count;
    return SQLITE_OK;
}

static inline char *
resolve_hidden_col_text(
    const vttp_vtab *vtab,
    uint icol,
    int argc,
    sqlite3_value **argv
) {
    int ai = vtab->icol_to_arg_index[icol];

    if (ai >= 0 && ai < argc) {
        return (char *) sqlite3_value_text(argv[ai]);
    }

    return hd(vtab->column_defs[icol].default_value);
}

static int xFilter(sqlite3_vtab_cursor *_cur,
                    int idxNum, const char *idxStr,
                    int argc, sqlite3_value **argv)
{
    vttp_vtab *vtab = (vttp_vtab*)_cur->pVtab;
    vttp_cursor_t *cur = (vttp_cursor_t*)_cur;

    cur->eof = 0, cur->count = 0, cur->next_doc = NULL;

    // Extract URL
    if (argc == 0 && !vtab->column_defs[ICOL_URL].default_value.hd) {
        _cur->pVtab->zErrMsg =
            sqlite3_mprintf("(vttp) need at least 1 argument or default url");
        return SQLITE_ERROR;
    }

    char *url = resolve_hidden_col_text(vtab, ICOL_URL, argc, argv);
    // char *headers = resolve_hidden_col_text(vtab, ICOL_HEADERS, argc, argv);
    char *body = resolve_hidden_col_text(vtab, ICOL_BODY, argc, argv);


    FILE *json_response = cookie(&COOKIE_JSON, NULL);
    cur->stream = fetch(url, (const char *[]){0}, json_response);

    char *errmsg = NULL;
    cur->next_doc = next_json_obj(cur->stream, &errmsg);

    if (!cur->next_doc) {
        _cur->pVtab->zErrMsg = errmsg;
        return SQLITE_ERROR;
    }

    return SQLITE_OK;
}

static sqlite3_module vttp = {
    .iVersion=0,
    .xCreate=vttpCreate,
    .xConnect=vttpConnect,
    .xBestIndex=vttpBestIndex,
    .xDisconnect=vttpDisconnect,
    .xDestroy=vttpDisconnect,
    .xOpen=vttpOpen,
    .xClose=vttpClose,
    .xFilter=xFilter,
    .xNext=vttpNext,
    .xEof=xEof,
    .xColumn=xColumn,
    .xRowid=xRowid,
    .xUpdate=NULL,
    .xBegin=NULL,
    .xSync=NULL,
    .xCommit=NULL,
    .xRollback=NULL,
    .xFindFunction=NULL
};

// Runtime loadable entry
int sqlite3_vttp_init(sqlite3 *db, char **pzErrMsg,
                       const sqlite3_api_routines *pApi) {
    SQLITE_EXTENSION_INIT2(pApi);
    // oh yeah baby
    int rc = sqlite3_create_module(db, "vttp", &vttp, 0);
    return rc;
}
