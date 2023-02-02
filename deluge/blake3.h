#if defined (__OPENCL_VERSION__)
#  ifndef _DELUGE_BLAKE3_H_BIN_
#    define _DELUGE_BLAKE3_H_BIN_
#    define __DELUGE_BLAKE3_H__
#  endif
#else
#  ifndef _DELUGE_BLAKE3_H_
#    define _DELUGE_BLAKE3_H_
#    define __DELUGE_BLAKE3_H__
#  endif
#endif


#ifdef __DELUGE_BLAKE3_H__
#undef __DELUGE_BLAKE3_H__


#include "deluge/opencl.h"
#include "deluge/uint.h"


typedef struct {
	uint32_t state[16];
} blake3_t;


static inline uint32_t blake3_rotr32(uint32_t w, uint32_t c)
{
	return (w >> c) | (w << (32 - c));
}

static inline void blake3_g(uint32_t *state,
			    size_t a, size_t b, size_t c, size_t d,
			    uint32_t x, uint32_t y)
{
	state[a] = state[a] + state[b] + x;
	state[d] = blake3_rotr32(state[d] ^ state[a], 16);
	state[c] = state[c] + state[d];
	state[b] = blake3_rotr32(state[b] ^ state[c], 12);
	state[a] = state[a] + state[b] + y;
	state[d] = blake3_rotr32(state[d] ^ state[a], 8);
	state[c] = state[c] + state[d];
	state[b] = blake3_rotr32(state[b] ^ state[c], 7);
}


#if !defined (__OPENCL_VERSION__)


/* #include "deluge/atomic.h" */


struct device;

/* struct blake3_program */
/* { */
/* 	struct device  *dev; */
/* 	cl_program      prog; */
/* 	size_t          hashsum_wg_size; */
/* 	size_t          hashsum_wg_max; */
/* 	size_t          hashsum_gmem_input_size; */
/* 	size_t          hashsum_gmem_output_size; */
/* 	size_t          hashsum_lmem_size; */
/* }; */

int init_blake3_program(cl_program *text, struct device *dev);


/* struct deluge; */
/* struct deluge_dispatch; */

/* int hashsum64_blake3_create(struct deluge *deluge, */
/* 			    struct deluge_dispatch **dispatch, */
/* 			    const void *key); */


/* int init_blake3_text(cl_program *text, struct device *backend); */


#endif  /* !defined (__OPENCL_VERSION__) */


#endif
