#ifndef _INCLUDE_DELUGE_H_
#define _INCLUDE_DELUGE_H_


#include <stddef.h>
#include <stdint.h>


#define DELUGE_SUCCESS       0  /* Operation successful */
#define DELUGE_FAILURE      -1  /* Implementation issue, use debug mode */
#define DELUGE_NODEV        -2  /* Not suitable device */
#define DELUGE_OUT_OF_GMEM  -3  /* Not enough device global memory */
#define DELUGE_OUT_OF_LMEM  -4  /* Not enough device local memory */
#define DELUGE_CANCEL       -5  /* Job canceled */


struct deluge;

/*
 * A deluge context.
 * Manage accelerator device resources and orchestrate between one or more
 * dispatches.
 */
typedef struct deluge *deluge_t;

/*
 * Create a new deluge context.
 * Allocate resources for an empty deluge context.
 * Return `DELUGE_SUCCESS` in case of success.
 */
int deluge_create(deluge_t *deluge);

/*
 * Destroy a deluge context.
 * The given deluge context is no more usable after this call.
 * The objects associated to this context are still usable.
 */
void deluge_destroy(deluge_t deluge);


/*
 * A highway hash and sum dispatch.
 * Dispach and schedule jobs between one or more accelerator devices.
 */
struct deluge_highway;

typedef struct deluge_highway *deluge_highway_t;

/*
 * Create a new highway hash and sum dispatch .
 * Compile the highway kernel program on all deluge devices and get them ready
 * to perform jobs with the given `key`.
 * Return `DELUGE_SUCCESS` in case of success.
 */
int deluge_highway_create(deluge_t deluge, deluge_highway_t *highway,
			  const uint64_t key[4]);

/*
 * Destroy the given highway dispatch.
 * The given dispatch is no more usable after this call.
 */
void deluge_highway_destroy(deluge_highway_t highway);

/*
 * Indicate how many highway hash and sum stations can be allocated on the
 * given dispatch.
 * Allocating more than the given number returns an error.
 * A dispatch cannot process jobs with less than a station.
 * More stations usually result in better throughput.
 */
size_t deluge_highway_space(deluge_highway_t highway);

/*
 * Allocate `len` stations on the given dispatch.
 * Return `DELUGE_SUCCESS` in case of success.
 */
int deluge_highway_alloc(deluge_highway_t highway, size_t len);

/*
 * Schedule a job on the given dispatch to do the follosing:
 * - perform a highway hash on each of the `nelem` elements in `elems`
 * - sum all the resulting `uint256_t` into a single `uint320_t`
 * - call the given `cb` callback giving it the following arguments:
 *   - the status of the job: `DELUGE_SUCCESS` in case of success
 *   - the resulting `uint320_t` in little endian
 *   - the given `user` argument
 * Return `DELUGE_SUCCESS` if the job has been successfully scheduled.
 */
int deluge_highway_schedule(deluge_highway_t highway, const uint64_t *elems,
			    size_t nelem, void (*cb)(int, uint64_t[5], void *),
			    void *user);


#endif
