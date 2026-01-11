.mode box
.load ./libvttp

CREATE VIRTUAL TABLE patients USING vttp (
    "resourceType" TEXT,
    id INT,
    gender TEXT
);

SELECT * FROM patients 
WHERE url = 'https://r4.smarthealthit.org/Patient' 
and body = (
    SELECT resource.*
    FROM json_each($->'entry') as entry,
         json_each(entry->'resource') as resource;
);
