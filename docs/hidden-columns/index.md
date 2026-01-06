import DocCardList from '@theme/DocCardList';

# Hidden Columns
SQLite allows for so-called [*hidden columns*](https://www.sqlite.org/vtab.html#hidden_columns_in_virtual_tables)
in virtual tables. Hidden columns are used by VTTP to resolve the dispatched HTTP request. As of the time of writing,
there are 3 hidden columns baked into every virtual table created using the `vttp` module:

<DocCardList />

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

```sql {4-6,13}
DROP TABLE IF EXISTS albums;

CREATE VIRTUAL TABLE albums USING vttp (
    url INT,
    headers FLOAT,
    body INT,
    id INT,
    "userId" INT,
    title TEXT
);

SELECT * FROM albums
WHERE url = 'https://jsonplaceholder.typicode.com/albums' -- url is still TEXT!
LIMIT 3;
```

... and VTTP will simply ignore the bad types.

```sql
sqlite> SELECT * FROM albums
        WHERE url = 'https://jsonplaceholder.typicode.com/albums'
        LIMIT 3;
┌────┬────────┬──────────────────────────────────┐
│ id │ userId │              title               │
├────┼────────┼──────────────────────────────────┤
│ 1  │ 1      │ quidem molestiae enim            │
│ 2  │ 1      │ sunt qui excepturi placeat culpa │
│ 3  │ 1      │ omnis laborum odio               │
└────┴────────┴──────────────────────────────────┘
```

Only constraints on the hidden columns are evaluated by the VTTP virtual table module.
