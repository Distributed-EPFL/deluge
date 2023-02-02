#ifndef _INCLUDE_DELUGE_H_
#define _INCLUDE_DELUGE_H_


#include <stddef.h>
#include <stdint.h>


#define DELUGE_SUCCESS       0  /* Operation successful */
#define DELUGE_FAILURE      -1  /* Implementation issue, use debug mode */
#define DELUGE_NODEV        -2  /* Not suitable device */
#define DELUGE_NOMEM        -3  /* Not enough device memory */
#define DELUGE_CANCEL       -4  /* Job canceled */


/*
 * Initialize deluge global management structures.
 * This is called implicitely by first usage to deluge routines.
 */
int deluge_init(void);

/*
 * Free deluge global management structures.
 * This should be used only before ending the program or before any deluge
 * routine is called for a long time.
 */
void deluge_finalize(void);


/*
 * Device job dispatcher.
 * The type of job it dispatches depends on how it was created.
 */
struct deluge_dispatch;

typedef struct deluge_dispatch *deluge_dispatch_t;

/*
 * Destroy the given `this` dispatcher.
 * The already scheduled jobs are canceled (i.e. they terminate with
 * `DELUGE_CANCEL`).
 * Internal resources of this dispatcher are freed.
 * Memory areas allocated from this dispatcher are still valid until freed.
 */
void deluge_dispatch_destroy(deluge_dispatch_t this);


#define DELUGE_HASHSUM64_LEN  40

int deluge_hashsum64_schedule(deluge_dispatch_t this,
			      const uint64_t *restrict elems, size_t nelem,
			      void (*cb)(int, void *, void *), void *user);


/*
 * Size of hashsum64 with blake3 key.
 */
#define DELUGE_BLAKE3_KEYLEN  32

/*
 * Create a dispatcher for hashsum64 jobs using the blake3 hashing algorithm.
 * The blake3 algorithm is salted with the provided `DELUGE_BLAKE3_KEYLEN` bits
 * long `key`.
 * On success, store the new dispatch in `*this` and return `DELUGE_SUCCESS`.
 */
int deluge_hashsum64_blake3_create(deluge_dispatch_t *restrict this,
				   const void *key);


#endif
