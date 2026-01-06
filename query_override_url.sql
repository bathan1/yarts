.load ./libvttp
.mode box

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
