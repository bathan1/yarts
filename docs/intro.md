---
sidebar_position: 1
---

# Introduction
*VTTP* is an SQLite runtime extension that provides an HTTP-backed virtual table.

Let's discover **vttp in less than 5 minutes**.

## Getting Started

Get started by **installing the latest extension binary**.

### What you'll need

- [SQLite](https://sqlite.org/) version 3.45 or higher.

## Link the Extension

Install the extension from the Github releases page and open SQLite. 
The default release is Linux x86_64:

```bash
curl -LO https://github.com/bathan1/vttp/releases/latest/download/libvttp.so
sqlite3
```

Load the extension:

```sql
.load ./libvttp
```

Optionally format the cli:

```sql
.mode box
```

You have now linked the `vttp` virtual table library with SQLite.

## Write your Queries
Create a Virtual Table by declaring your expected payload shape with the `vttp` virtual table:

```sql
CREATE VIRTUAL TABLE todos USING vttp (
    id INT,
    "userId" INT,
    title TEXT,
    completed TEXT
);
```

The `vttp` module will include a `url HIDDEN TEXT` column into your virtual table, which specifies the url 
to send the http request to.

To fetch `todos` from a json dummy api, for example, we set `url` equal to the 
endpoint in `SELECT ... WHERE ...` query:

```sql
SELECT * FROM todos WHERE url = 'https://jsonplaceholder.typicode.com/todos' LIMIT 5;
```

```sql
sqlite> SELECT * FROM todos WHERE url = 'https://jsonplaceholder.typicode.com/todos' LIMIT 5;
┌────┬────────┬──────────────────────────────────────────────────────────────┬───────────┐
│ id │ userId │                            title                             │ completed │
├────┼────────┼──────────────────────────────────────────────────────────────┼───────────┤
│ 1  │ 1      │ delectus aut autem                                           │ false     │
├────┼────────┼──────────────────────────────────────────────────────────────┼───────────┤
│ 2  │ 1      │ quis ut nam facilis et officia qui                           │ false     │
├────┼────────┼──────────────────────────────────────────────────────────────┼───────────┤
│ 3  │ 1      │ fugiat veniam minus                                          │ false     │
├────┼────────┼──────────────────────────────────────────────────────────────┼───────────┤
│ 4  │ 1      │ et porro tempora                                             │ true      │
├────┼────────┼──────────────────────────────────────────────────────────────┼───────────┤
│ 5  │ 1      │ laboriosam mollitia et enim quasi adipisci quia provident il │ false     │
│    │        │ lum                                                          │           │
└────┴───────────────────────────────────────────────────────────────────────┴───────────┘
```

Here's how we can filter for completed todos only:

```sql
SELECT * FROM todos WHERE url = 'https://jsonplaceholder.typicode.com/todos'
AND completed = 'true' LIMIT 5;
```

We have to set `completed` to a `TEXT` as opposed to a `BOOL`, because
there is no `BOOL` in SQLite. The full notes on JSON type conversions
can be found [here](./read-json), but basically just worry about the SQL types;
VTTP will handle the type conversions into SQLite for you.

```sql
sqlite> SELECT * FROM todos WHERE url = 'https://jsonplaceholder.typicode.com/todos'
        AND completed = 'true' LIMIT 5;
┌────┬────────┬──────────────────────────────────────────────┬───────────┐
│ id │ userId │                    title                     │ completed │
├────┼────────┼──────────────────────────────────────────────┼───────────┤
│ 4  │ 1      │ et porro tempora                             │ true      │
│ 8  │ 1      │ quo adipisci enim quam ut ab                 │ true      │
│ 10 │ 1      │ illo est ratione doloremque quia maiores aut │ true      │
│ 11 │ 1      │ vero rerum temporibus dolor                  │ true      │
│ 12 │ 1      │ ipsa repellendus fugit nisi                  │ true      │
└────┴────────┴──────────────────────────────────────────────┴───────────┘
```

If you only cared about the `id` and `title` fields, simply omit the other fields from the `create virtual table`
statement:

```sql
DROP TABLE IF EXISTS todos;

CREATE VIRTUAL TABLE todos USING vttp (
    id INT,
    title TEXT
);

SELECT * FROM todos WHERE url = 'https://jsonplaceholder.typicode.com/todos' LIMIT 5;
```

```sql
sqlite> SELECT * FROM todos WHERE url = 'https://jsonplaceholder.typicode.com/todos' LIMIT 5;
┌────┬──────────────────────────────────────────────────────────────┐
│ id │                            title                             │
├────┼──────────────────────────────────────────────────────────────┤
│ 1  │ delectus aut autem                                           │
├────┼──────────────────────────────────────────────────────────────┤
│ 2  │ quis ut nam facilis et officia qui                           │
├────┼──────────────────────────────────────────────────────────────┤
│ 3  │ fugiat veniam minus                                          │
├────┼──────────────────────────────────────────────────────────────┤
│ 4  │ et porro tempora                                             │
├────┼──────────────────────────────────────────────────────────────┤
│ 5  │ laboriosam mollitia et enim quasi adipisci quia provident il │
│    │ lum                                                          │
└────┴──────────────────────────────────────────────────────────────┘
```

:::tip

Since `url` is a hidden column, you can query the url column
with the table valued function sugar syntax.

The equivalent query to above using this sugar syntax is:

```sql
SELECT * FROM todos('https://jsonplaceholder.typicode.com/todos') LIMIT 5;
```

:::


If you're fetching from a single server, you can set a default value 
for the `url` column in the `create virtual table` statement:

```sql
DROP TABLE IF EXISTS todos;

CREATE VIRTUAL TABLE todos USING vttp (
    url TEXT DEFAULT 'https://jsonplaceholder.typicode.com/todos',
    id INT,
    title TEXT
);
```

This way, you don't need to set the `url` column for each query:

```sql
SELECT * FROM todos LIMIT 5;
```

For the most part, virtual tables created with VTTP can be queried as if they are plain SQL
and (tries to) keeps the number of things you need to learn to use it to a minimum.

:::important
In general, you *should* set a default URL for `vttp` virtual tables 
if you're only querying 1 endpoint for a `vttp` virtual table.
:::
