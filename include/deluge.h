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

typedef struct deluge *deluge_t;

/*
 * Create a new deluge context.
 * Allocate resources for an empty deluge context.
 * Return `DELUGE_SUCCESS` in case of success.
 */
int deluge_create(deluge_t *deluge);

/*
 * Destroy a deluge context.
 * Make the deluge context unusable.
 * When all the compute stations associated to this context have been destroyed
 * then free the resources of this context.
 */
void deluge_destroy(deluge_t deluge);


struct deluge_highway;

typedef struct deluge_highway *deluge_highway_t;

/*
 * Create a new deluge highway context.
 * Compile the highway kernel program on all deluge devices.
 * Return 0 in case of success, otherwise set the deluge error appropriately.
 */
int deluge_highway_create(deluge_t deluge, deluge_highway_t *highway,
			  const uint64_t key[4]);

void deluge_highway_destroy(deluge_highway_t highway);

size_t deluge_highway_space(deluge_highway_t highway);

int deluge_highway_alloc(deluge_highway_t highway, size_t len);

int deluge_highway_schedule(deluge_highway_t highway, const uint64_t *elems,
			    size_t nelem, void (*cb)(int, uint64_t[5], void *),
			    void *user);


#endif
