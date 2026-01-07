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

The JSON data in the payload contains 200 [Patient](https://hl7.org/fhir/R4/patient.html) objects in 
an array that we want to query as rows. But if we tried to run a `SELECT` against this...

```sql
SELECT * FROM patients LIMIT 5;
```

We'll end up with 1 row because VTTP treats single object bodies are treated as an single-element list:

```sql
sqlite> SELECT * FROM patients LIMIT 5;
┌──────────────┬──────────────────────────────────────┬──────┐
│ resourceType │                  id                  │ name │
├──────────────┼──────────────────────────────────────┼──────┤
│ Bundle       │ bccb2c06-2eb9-4589-aa3a-0aec1818f131 │      │
└──────────────┴──────────────────────────────────────┴──────┘
```

VTTP provides a way to get around this with the `body` hidden column.
We can specify the *nested* value we want to turn into rows by setting
the `url` column equal to a [JSONPath](https://en.wikipedia.org/wiki/JSONPath) string.

For our `patients`, that value is `'$.entry[*].resource'`. So by setting `body` 
equal to that like this:

```sql {2}
SELECT COUNT(*) as count, * FROM patients
WHERE body = '$.entry[*].resource'
LIMIT 5;
```

We will be able to view our patient rows:

```sql
sqlite> SELECT COUNT(*), * FROM patients
        WHERE body = '$.entry[*].resource'
        LIMIT 5;
```

