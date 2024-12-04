-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION test_dsm_registry" to load this file. \quit

CREATE FUNCTION set_val_in_shmem(val INT) RETURNS VOID
AS 'MODULE_PATHNAME' LANGUAGE C;

CREATE FUNCTION get_val_in_shmem() RETURNS INT
AS 'MODULE_PATHNAME' LANGUAGE C;

CREATE FUNCTION append_msg(text) RETURNS text
AS 'MODULE_PATHNAME' LANGUAGE C;

CREATE FUNCTION hash_size() RETURNS int
AS 'MODULE_PATHNAME' LANGUAGE C;

CREATE FUNCTION hash_put_int(text, integer) RETURNS int
AS 'MODULE_PATHNAME' LANGUAGE C;