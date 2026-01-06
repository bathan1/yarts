---
sidebar_position: 2
---

# Hidden Columns
SQLite allows for so-called [*hidden columns*](https://www.sqlite.org/vtab.html#hidden_columns_in_virtual_tables)
in virtual tables. Hidden columns are used by VTTP to resolve the dispatched HTTP request. As of the time of writing,
there are 3 hidden columns baked into every virtual table created using the `vttp` module:

1. [`url`](#url)
2. [`headers`](#headers)
3. [`body`](#body)

These columns are effectively plastered onto your `create virtual table` schema. 

For example, if we declared the following `albums` table:

```sql
CREATE VIRTUAL TABLE albums USING vttp (
    id INT,
    "userId" INT,
    title TEXT
);
```

The full table definition that SQLite sees will look something like this:

```sql {2-4}
CREATE TABLE albums (
    url TEXT,
    headers TEXT,
    body TEXT,
    id INT,
    "userId" INT,
    title TEXT
);
```

These columns' names and types are **fixed** with respect to the user's schema; that is,
they will *always* be included in the final table schema regardless of the user's declared schema.

Explicitly included hidden columns are simply ignored, *unless* they have some extra constraint (see the
below sections). For instance, you could have included the hidden columns explicitly in the `albums` table 
and effectively end up with the same table as before:

```sql {2-4}
CREATE VIRTUAL TABLE albums USING vttp (
    url TEXT,
    headers TEXT,
    body TEXT,
    id INT,
    "userId" INT,
    title TEXT
);
```

And not saying you should do this, but you *could* mismatch the types...

```sql {2-4}
CREATE VIRTUAL TABLE albums USING vttp (
    url INT,
    headers FLOAT,
    body INT,
    id INT,
    "userId" INT,
    title TEXT
);
```

... and VTTP would simply ignore the bad types.

As mentioned above, only constraints on the hidden columns are evaluated by the VTTP virtual table module,
which we'll go over here.

## URL
The first and the only required hidden column is the `url` column. Use this to specify the endpoint of the
server you are fetching from. By "required", I mean that it needs to be set equal to a value in each `SELECT`
query.

So to query the `albums` table from above, we need to specify the `url` value in the `WHERE` clause:

```sql {2}
SELECT * FROM albums
WHERE url = 'https://jsonplaceholder.typicode.com/albums';
```

VTTP will throw an error if you forget to set `url`:

```sql
sqlite> SELECT * FROM albums;
Parse error: (vttp) Missing `WHERE url = ...` (no default URL).
```

You don't need to specify `WHERE url = ...` if you provide
a `DEFAULT` value for `url` in the `create virtual table` statement:

```sql {4}
DROP TABLE IF EXISTS albums;

CREATE VIRTUAL TABLE albums USING vttp (
    url TEXT DEFAULT 'https://jsonplaceholder.typicode.com/albums',
    id INT,
    "userId" INT,
    title TEXT
);

SELECT * FROM albums LIMIT 3;
```

Then you won't need to filter by `url` each time:

```sql
sqlite> SELECT * FROM albums LIMIT 3;
┌────┬────────┬──────────────────────────────────┐
│ id │ userId │              title               │
├────┼────────┼──────────────────────────────────┤
│ 1  │ 1      │ quidem molestiae enim            │
│ 2  │ 1      │ sunt qui excepturi placeat culpa │
│ 3  │ 1      │ omnis laborum odio               │
└────┴────────┴──────────────────────────────────┘
```

If you set a `DEFAULT` value *and* specify a `url` in a `WHERE` clause, the `WHERE url = ...`
value will be used over the default

```sql {3}
DROP TABLE IF EXISTS todos;

CREATE VIRTUAL TABLE todos USING vttp (
    url text default 'https://jsonplaceholder.typicode.com/todos',
    "userId" int,
    id int,
    title text,
    completed text
);

SELECT * FROM todos
WHERE url = 'https://dummy-json.mock.beeceptor.com/todos';
```

## Headers

## Body
