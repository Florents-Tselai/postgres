CREATE EXTENSION test_dsm_registry;

SELECT hash_size();
SELECT hash_put_int('key1', 1);
SELECT hash_put_int('key2', 1);

-- switch database the hash should be the same
\c
SELECT hash_size();

