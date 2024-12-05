CREATE EXTENSION test_dsm_registry;
SELECT set_val_in_shmem(1236);
-- SELECT insert_into_htab(1, 100);  -- Insert key 1 with value 100
-- SELECT get_from_htab(1);  -- Should return 100

\c
SELECT get_val_in_shmem();
SELECT set_val_in_shmem(1236);
--
-- SELECT insert_into_htab(1, 100);  -- Insert key 1 with value 100
-- SELECT get_from_htab(1);  -- Should return 100
--
-- -- Insert test data into the hash table
-- SELECT insert_into_htab(1, 100);  -- Insert key 1 with value 100
-- SELECT insert_into_htab(2, 200);  -- Insert key 2 with value 200
-- SELECT insert_into_htab(3, 300);  -- Insert key 3 with value 300
--
-- -- Retrieve values from the hash table using `get_from_htab`
-- SELECT get_from_htab(1);  -- Should return 100
-- SELECT get_from_htab(2);  -- Should return 200
-- SELECT get_from_htab(3);  -- Should return 300