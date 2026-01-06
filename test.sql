.load ./libyarts
.mode box
create virtual table todos using fetch (
    url text default 'https://jsonplaceholder.typicode.com/todos',
    id int,
    "userId" int,
    title text,
    completed text
);
select * from todos limit 5;

CREATE VIRTUAL TABLE users USING fetch (
    url TEXT DEFAULT 'https://jsonplaceholder.typicode.com/users',
    id INT,
    name TEXT,
    address_street TEXT GENERATED ALWAYS AS (address->'street'),
    address_zipcode TEXT GENERATED ALWAYS AS (address->'zipcode'),
    address_geo_lat TEXT GENERATED ALWAYS AS (address->'geo'->'lat'),
    address_geo_lng TEXT GENERATED ALWAYS AS (address->'geo'->'lng')
);
SELECT * FROM users LIMIT 5;
