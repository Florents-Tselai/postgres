/*--------------------------------------------------------------------------
 *
 * test_dsm_registry.c
 *	  Test the dynamic shared memory registry.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		src/test/modules/test_dsm_registry/test_dsm_registry.c
 *
 * -------------------------------------------------------------------------
 */
#include "postgres.h"

#include "funcapi.h"
#include "storage/lwlock.h"
#include "utils/builtins.h"
#include "storage/dsm_registry.h"
#include "utils/hsearch.h"
#include "storage/shmem.h"

PG_MODULE_MAGIC;

#define HASH_KEYLEN 64	/* Maybe too conservative? */

typedef struct TestDSMRegistryStruct
{
	LWLock		lck;

	int			val;
	char		*msg;
	int			msglen;
} TestDSMRegistryStruct;

static HTAB *hash = NULL;

static TestDSMRegistryStruct *tdr_state;

static void
tdr_init_shmem(void *ptr)
{
	TestDSMRegistryStruct *state = (TestDSMRegistryStruct *) ptr;

	LWLockInitialize(&state->lck, LWLockNewTrancheId());
	state->val = 0;

	state->msg = palloc(1);
	state->msg[0] = '\0';
	state->msglen = 1;

	printf("init msg=%s\n", state->msg);
}

typedef enum valType
{
	TYPE_UNKNOWN = -1,
	TYPE_INTEGER = 0,
	TYPE_STRING = 1
} valType;

typedef struct hashEntry
{
	char key[HASH_KEYLEN];	/* MUST BE FIRST */

	valType typ;
	int val;
} hashEntry;


static void
tdr_attach_shmem(void)
{
	bool		found;

	tdr_state = GetNamedDSMSegment("test_dsm_registry",
								   sizeof(TestDSMRegistryStruct),
								   tdr_init_shmem,
								   &found);
	LWLockRegisterTranche(tdr_state->lck.tranche, "test_dsm_registry");

	if (hash == NULL)
	{
		Assert (hash == NULL);
		HASHCTL info;
		info.keysize = HASH_KEYLEN;
		info.entrysize = sizeof(hashEntry);
		info.hcxt = CurrentMemoryContext;
		hash = ShmemInitHash("shmem hash", 10, 100, &info,
			HASH_ELEM | HASH_STRINGS | HASH_CONTEXT);
	}

}

PG_FUNCTION_INFO_V1(hash_size);
Datum
hash_size(PG_FUNCTION_ARGS)
{

	long result;
	tdr_attach_shmem();

	LWLockAcquire(&tdr_state->lck, LW_SHARED);
	result = hash_get_num_entries(hash);
	LWLockRelease(&tdr_state->lck);

	PG_RETURN_INT64(result);
}

PG_FUNCTION_INFO_V1(hash_put_int);
Datum
hash_put_int(PG_FUNCTION_ARGS)
{
	text *t_key = PG_GETARG_TEXT_PP(0);
	char *key	= text_to_cstring(t_key);	/* Guaranteed null-terminated */
	int key_len = VARSIZE_ANY_EXHDR(t_key);
	int	value	= PG_GETARG_INT32(1);

	Assert(key_len <= HASH_KEYLEN);

	hashEntry	*entry;
	bool		found;

	tdr_attach_shmem();

	LWLockAcquire(&tdr_state->lck, LW_EXCLUSIVE);

	entry = hash_search(hash, key, HASH_ENTER, &found);
	if (!found)
	{
		entry->typ = TYPE_INTEGER;
		entry->val = value;
	}

	LWLockRelease(&tdr_state->lck);

	/* Make sure */
	Assert(sizeof(*entry) == HASH_KEYLEN + sizeof(valType) + sizeof(int));
	Assert(entry->typ == TYPE_INTEGER);


	PG_RETURN_VOID();
}

PG_FUNCTION_INFO_V1(hash_get_int);
Datum
hash_get_int(PG_FUNCTION_ARGS)
{
	text   *t_key = PG_GETARG_TEXT_PP(0);
	char   *key   = text_to_cstring(t_key); /* Guaranteed null-terminated */
	int     key_len = VARSIZE_ANY_EXHDR(t_key);
	int     result;

	hashEntry *entry;
	bool found;

	/* Ensure the key length is within permissible bounds */
	if (key_len > HASH_KEYLEN)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("key length exceeds maximum allowed length of %d", HASH_KEYLEN)));

	/* Attach to shared memory and acquire lock */
	tdr_attach_shmem();

	LWLockAcquire(&tdr_state->lck, LW_SHARED);

	/* Search for the key in the hash table */
	entry = hash_search(hash, key, HASH_FIND, &found);
	if (found)
	{
		if (entry->typ != TYPE_INTEGER)
		{
			LWLockRelease(&tdr_state->lck); /* Release lock before raising error */
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("value for key \"%s\" is not an integer", key)));
		}
		result = entry->val;
	}
	LWLockRelease(&tdr_state->lck);

	/* If the key was not found, return NULL */
	if (!found)
		PG_RETURN_NULL();

	/* Return the integer value */
	PG_RETURN_INT32(result);
}

PG_FUNCTION_INFO_V1(hash_type);
Datum
hash_type(PG_FUNCTION_ARGS)
{
	text *t_key = PG_GETARG_TEXT_PP(0);
	char *key	= text_to_cstring(t_key);	/* Guaranteed null-terminated */
	int key_len = VARSIZE_ANY_EXHDR(t_key);

	valType result;

	Assert(key_len <= HASH_KEYLEN);

	hashEntry	*entry;
	bool		found;

	tdr_attach_shmem();

	LWLockAcquire(&tdr_state->lck, LW_SHARED);

	entry = hash_search(hash, key, HASH_FIND, &found);
	result = (found) ? entry->typ : TYPE_UNKNOWN;

	LWLockRelease(&tdr_state->lck);

	switch (result)
	{
		case TYPE_INTEGER:
			PG_RETURN_TEXT_P(cstring_to_text("integer"));
		case TYPE_STRING:
			PG_RETURN_TEXT_P(cstring_to_text("string"));
		case TYPE_UNKNOWN:
			PG_RETURN_TEXT_P(cstring_to_text("unknown"));
		default:
			PG_RETURN_TEXT_P(cstring_to_text("unknown"));
	}
}

