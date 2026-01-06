.load ./libvttp
.mode box

CREATE VIRTUAL TABLE albums USING vttp (
    url text default 'https://jsonplaceholder.typicode.com/albums',
    id int,
    "userId" int,
    title text
);

SELECT * FROM albums LIMIT 3;
