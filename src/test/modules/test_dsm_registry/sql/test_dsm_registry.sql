CREATE EXTENSION test_dsm_registry;

/* integers */
SELECT hash_size();
SELECT hash_put_int('key1', 1);
SELECT hash_put_int('key2', 2);
SELECT hash_get_int('key2');
SELECT hash_get_int('sdfgdfg');
SELECT hash_size();

SELECT hash_type('key1');
SELECT hash_type('gdgf');

SELECT hash_size(); -- 2
-- switch database the hash should be the same
\c
SELECT hash_size(); -- 2
SELECT hash_put_int('key3', 1);
SELECT hash_size(); -- 3

/* strings */
SELECT hash_put_string('str1', 'strval1 '); -- note trailing space
SELECT hash_size(); -- 4
SELECT hash_get_string('str1');


