---
sidebar_position: 1
---
# url
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
value will be used over the default:

```sql {4,12}
DROP TABLE IF EXISTS todos;

CREATE VIRTUAL TABLE todos USING vttp (
    url TEXT default 'https://jsonplaceholder.typicode.com/todos',
    "userId" INT,
    id INT,
    title TEXT,
    completed TEXT
);

SELECT * FROM todos
WHERE url = 'https://dummy-json.mock.beeceptor.com/todos';
```

This will select the todos from the https://dummy-json.mock.beeceptor.com/todos url instead
of the `DEFAULT` typicode api:

```sql
sqlite> SELECT * FROM todos 
        WHERE url = 'https://dummy-json.mock.beeceptor.com/todos';
┌────────┬────┬──────────────────┬───────────┐
│ userId │ id │      title       │ completed │
├────────┼────┼──────────────────┼───────────┤
│ 3      │ 1  │ Buy groceries    │ false     │
│ 5      │ 2  │ Go for a walk    │ true      │
│ 1      │ 3  │ Finish homework  │ false     │
│ 7      │ 4  │ Read a book      │ true      │
│ 2      │ 5  │ Pay bills        │ false     │
│ 4      │ 6  │ Call a friend    │ true      │
│ 6      │ 7  │ Clean the house  │ false     │
│ 9      │ 8  │ Go to the gym    │ true      │
│ 8      │ 9  │ Write a report   │ false     │
│ 2      │ 10 │ Cook dinner      │ true      │
│ 1      │ 11 │ Study for exams  │ false     │
│ 3      │ 12 │ Do laundry       │ true      │
│ 5      │ 13 │ Practice guitar  │ false     │
│ 7      │ 14 │ Plan a trip      │ true      │
│ 9      │ 15 │ Attend a meeting │ false     │
│ 6      │ 16 │ Water the plants │ true      │
│ 4      │ 17 │ Fix the car      │ false     │
│ 8      │ 18 │ Watch a movie    │ true      │
│ 10     │ 19 │ Visit the museum │ false     │
│ 1      │ 20 │ Send an email    │ true      │
└────────┴────┴──────────────────┴───────────┘
```

