-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION test_dsm_registry" to load this file. \quit

CREATE FUNCTION hash_size() RETURNS int
AS 'MODULE_PATHNAME' LANGUAGE C;

CREATE FUNCTION hash_put_int(text, integer) RETURNS void
AS 'MODULE_PATHNAME' LANGUAGE C;

CREATE FUNCTION hash_get_int(text) RETURNS integer
AS 'MODULE_PATHNAME' LANGUAGE C;

CREATE FUNCTION hash_type(text) RETURNS text
AS 'MODULE_PATHNAME' LANGUAGE C;

CREATE FUNCTION hash_put_string(text, text) RETURNS void
AS 'MODULE_PATHNAME' LANGUAGE C;

CREATE FUNCTION hash_get_string(text) RETURNS text
AS 'MODULE_PATHNAME' LANGUAGE C;
