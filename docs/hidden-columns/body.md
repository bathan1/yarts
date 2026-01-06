---
sidebar_position: 3
---
# body
The `body` hidden column specifies how to transform incoming response bodies
for VTTP virtual tables. It also sets the expected data encoding for the response.

As of the time of writing, the only supported body encoding is JSON, though
other path selectors may be added in the future.

## JSON
The `body` column is useful for extracting nested `object array` values that you
want to project as rows.

Consider the following `patients` table that we want to fetch from the
Smart Health IT mock R4 FHIR API:

```sql
CREATE VIRTUAL TABLE patients USING vttp (
    url TEXT DEFAULT 'https://r4.smarthealthit.org/Patient',
    "resourceType" TEXT,
    id TEXT,
    name TEXT
);
```

If we tried to run a `SELECT` against this...

```sql
SELECT * FROM patients LIMIT 5;
```

We'll end up with 1 row (single object bodies are treated as a single row):

```sql

```
