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

#include "fmgr.h"
#include "storage/dsm_registry.h"
#include "storage/lwlock.h"
#include "lib/dshash.h"
#include "common/hashfn.h"

PG_MODULE_MAGIC;

typedef struct TestDSMRegistryStruct
{
	int					val;
	dsa_handle			dsa_handle;	/* Attach to this when creating dshash */
	dshash_table		*htab;		/* Attach to this when creating dshash */
	LWLock				lck;
} TestDSMRegistryStruct;

/* Key for the hash table */
typedef struct TestHashKey
{
	int key;
} TestHashKey;

/* Value for the hash table */
typedef struct TestHashEntry
{
	TestHashKey key; /* Must be the first field */
	int value;
} TestHashEntry;

static TestDSMRegistryStruct *tdr_state;

/* Compare function for the hash table */
static int
test_hash_compare(const void *key1, const void *key2, size_t size, void *arg)
{
	return memcmp(key1, key2, size);
}

/* Hash function for the hash table */
static uint32
test_hash_function(const void *key, size_t size, void *arg)
{
	/* Use Postgres's hash_any function for hashing */
	return hash_any((const unsigned char *) key, size);
}

/* Copy function for the hash table */
static void
test_hash_copy(void *dest, const void *src, size_t size, void *arg)
{
	memcpy(dest, src, size);
}

/* Define hash table parameters */
static dshash_parameters hash_params = {
	.key_size = sizeof(TestHashKey),
	.entry_size = sizeof(TestHashEntry),
	.compare_function = test_hash_compare,
	.hash_function = test_hash_function,
	.copy_function = test_hash_copy
};



/* Initialize shared memory */
static void
tdr_init_shmem(void *ptr)
{
	TestDSMRegistryStruct *state = (TestDSMRegistryStruct *) ptr;
	LWLockInitialize(&state->lck, LWLockNewTrancheId());
	state->val = 0;
	state->dsa_handle = DSHASH_HANDLE_INVALID;

	dsa_area *dsa = dsa_create(state->lck.tranche);
	state->dsa_handle  = dsa_get_handle(dsa);
	dsa_pin(dsa);

	state->htab = dshash_create(dsa, &hash_params, NULL);
	printf("htab created at %d\n", state->dsa_handle);
	printf("dshash pointer =%p\n", state->htab);

}


static void
tdr_attach_shmem(void)
{
	bool		found;
	tdr_state = GetNamedDSMSegment("test_dsm_registry",
								   sizeof(TestDSMRegistryStruct),
								   tdr_init_shmem,
								   &found);
	LWLockRegisterTranche(tdr_state->lck.tranche, "test_dsm_registry");

}


PG_FUNCTION_INFO_V1(set_val_in_shmem);
Datum
set_val_in_shmem(PG_FUNCTION_ARGS)
{
	tdr_attach_shmem();

	LWLockAcquire(&tdr_state->lck, LW_EXCLUSIVE);
	printf("dsa_handle=%d\n", tdr_state->dsa_handle);

	tdr_state->val = PG_GETARG_UINT32(0);
	printf("dshash pointer =%p\n", tdr_state->htab);


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
	printf("dsa_handle=%d\n", tdr_state->dsa_handle);
	ret = tdr_state->val;

	LWLockRelease(&tdr_state->lck);

	PG_RETURN_UINT32(ret);
}

// PG_FUNCTION_INFO_V1(insert_into_htab);
// Datum
// insert_into_htab(PG_FUNCTION_ARGS)
// {
// 	int key = PG_GETARG_INT32(0);
// 	int value = PG_GETARG_INT32(1);
// 	TestHashKey hash_key;
// 	TestHashEntry *entry;
// 	bool found;
//
// 	tdr_attach_shmem();
// 	LWLockAcquire(&tdr_state->lck, LW_EXCLUSIVE);
//
// 	/* Prepare the hash key */
// 	hash_key.key = key;
//
// 	/* Insert into the hash table */
// 	entry = (TestHashEntry *) dshash_find_or_insert(tdr_state->htab, &hash_key, &found);
// 	entry->value = value; /* Set or update the value */
// 	printf("inserted entry key=%d, value=%d\n", entry->key, entry->value);
// 	/* Release the entry lock */
// 	dshash_release_lock(tdr_state->htab, entry);
//
// 	LWLockRelease(&tdr_state->lck);
//
// 	PG_RETURN_VOID();
// }
//
// PG_FUNCTION_INFO_V1(get_from_htab);
// Datum
// get_from_htab(PG_FUNCTION_ARGS)
// {
// 	int key = PG_GETARG_INT32(0);
// 	TestHashKey hash_key;
// 	TestHashEntry *entry;
// 	int value;
//
// 	tdr_attach_shmem();
//
// 	LWLockAcquire(&tdr_state->lck, LW_SHARED);
//
// 	/* Prepare the hash key */
// 	hash_key.key = key;
//
// 	/* Look up the hash table */
// 	entry = (TestHashEntry *) dshash_find(tdr_state->htab, &hash_key, false);
//
// 	/* If not found, raise an error */
// 	if (!entry)
// 		ereport(ERROR, (errmsg("Key not found in hash table")));
//
// 	/* Get the value */
// 	value = entry->value;
//
// 	/* Release the entry lock */
// 	dshash_release_lock(tdr_state->htab, entry);
// 	LWLockRelease(&tdr_state->lck);
//
//
// 	PG_RETURN_INT32(value);
// }