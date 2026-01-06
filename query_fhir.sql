.mode box
.load ./libvttp

CREATE VIRTUAL TABLE patients USING vttp (
    url TEXT DEFAULT 'https://r4.smarthealthit.org/Patient',
    "resourceType" TEXT,
    id INT,
    name TEXT
);

SELECT * FROM patients;
