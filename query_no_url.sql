.load ./libvttp
.mode box
CREATE VIRTUAL TABLE albums USING vttp (
    id INT,
    "userId" INT,
    title TEXT
);
SELECT * FROM albums;

