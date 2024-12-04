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

#define HASH_MAX_KEYLEN NAMEDATALEN

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


typedef struct hashKey
{
	char key[HASH_MAX_KEYLEN + 1];
} hashKey;

typedef struct hashEntry
{
	hashKey key;	/* MUST BE FIRST */

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

	printf("shm attached\n");
	if (hash == NULL)
	{
		Assert (hash == NULL);
		printf("first time\t initializing hash too\n");
		HASHCTL info;
		info.keysize = sizeof(hashKey);
		info.entrysize = sizeof(hashEntry);
		info.hcxt = CurrentMemoryContext;
		hash = ShmemInitHash("shmem hash", 10, 100, &info,
			HASH_ELEM | HASH_STRINGS | HASH_CONTEXT);

		printf("hash initialized num_entries=%ld\n", hash_get_num_entries(hash));
	}

}

PG_FUNCTION_INFO_V1(set_val_in_shmem);
Datum
set_val_in_shmem(PG_FUNCTION_ARGS)
{
	tdr_attach_shmem();

	LWLockAcquire(&tdr_state->lck, LW_EXCLUSIVE);
	tdr_state->val = PG_GETARG_UINT32(0);
	LWLockRelease(&tdr_state->lck);

	PG_RETURN_VOID();
}

PG_FUNCTION_INFO_V1(get_val_in_shmem);
Datum
get_val_in_shmem(PG_FUNCTION_ARGS)
{
	int			ret;

	tdr_attach_shmem();

	LWLockAcquire(&tdr_state->lck, LW_SHARED);
	ret = tdr_state->val;
	LWLockRelease(&tdr_state->lck);

	PG_RETURN_UINT32(ret);
}

PG_FUNCTION_INFO_V1(append_msg);

Datum
append_msg(PG_FUNCTION_ARGS)
{
	char       *new_msg = text_to_cstring(PG_GETARG_TEXT_PP(0));
	int         new_msg_len = strlen(new_msg);
	tdr_attach_shmem();

	LWLockAcquire(&tdr_state->lck, LW_EXCLUSIVE);

	int total_len = tdr_state->msglen + new_msg_len;

	tdr_state->msg = repalloc(tdr_state->msg, total_len + 1);
	memcpy(tdr_state->msg + tdr_state->msglen, new_msg, new_msg_len + 1);  /* +1 to include null terminator */
	tdr_state->msglen = total_len;

	/* Print the new message for debugging */
	printf("msg=%s\n", tdr_state->msg);

	/* Release the lock */
	LWLockRelease(&tdr_state->lck);

	/* Return the updated message */
	PG_RETURN_TEXT_P(cstring_to_text(tdr_state->msg));
}

PG_FUNCTION_INFO_V1(hash_size);

Datum
hash_size(PG_FUNCTION_ARGS)
{

	tdr_attach_shmem();

	PG_RETURN_INT32(hash_get_num_entries(hash));
}

PG_FUNCTION_INFO_V1(hash_put_int);
Datum
hash_put_int(PG_FUNCTION_ARGS)
{
	text		*key	= PG_GETARG_TEXT_P(0);
	int	value	= PG_GETARG_INT32(1);

	int64 result;

	hashEntry	*entry;
	bool found;


	Assert(VARSIZE_ANY_EXHDR(key) <= HASH_MAX_KEYLEN);

	if (VARSIZE_ANY_EXHDR(key) > HASH_MAX_KEYLEN)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("hash key length %ld, exceeds max supported %d", VARSIZE_ANY_EXHDR(key), HASH_MAX_KEYLEN)));

	tdr_attach_shmem();

	entry = hash_search(hash, VARDATA_ANY(key), HASH_ENTER, &found);
	if (!found)
	{
		entry->val = value;
	}

	result = hash_get_num_entries(hash);

	PG_RETURN_INT64(result);
}

