#if defined (__OPENCL_VERSION__)
#  ifndef _DELUGE_HIGHWAY_H_BIN_
#    define _DELUGE_HIGHWAY_H_BIN_
#    define __DELUGE_HIGHWAY_H__
#  endif
#else
#  ifndef _DELUGE_HIGHWAY_H_
#    define _DELUGE_HIGHWAY_H_
#    define __DELUGE_HIGHWAY_H__
#  endif
#endif


#ifdef __DELUGE_HIGHWAY_H__
#undef __DELUGE_HIGHWAY_H__


#include "deluge/opencl.h"
#include "deluge/uint.h"


typedef struct {
        uint64_t v0[4];
        uint64_t v1[4];
        uint64_t mul0[4];
        uint64_t mul1[4];
} highway_t;


#if !defined (__OPENCL_VERSION__)


#include "deluge/atomic.h"


struct device;

struct highway_program
{
	struct device  *dev;
	cl_program      prog;
	size_t          hashsum_wg_size;
	size_t          hashsum_wg_max;
	size_t          hashsum_gmem_input_size;
	size_t          hashsum_gmem_output_size;
	size_t          hashsum_lmem_size;
};

int init_highway_program(struct highway_program *this, struct device *dev);

void finlz_highway_program(struct highway_program *this);


#endif  /* !defined (__OPENCL_VERSION__) */


#endif
