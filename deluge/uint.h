#if defined (__OPENCL_VERSION__)
#  ifndef _DELUGE_UINT_H_BIN_
#    define _DELUGE_UINT_H_BIN_
#    define __DELUGE_UINT_H__
#  endif
#else
#  ifndef _DELUGE_UINT_H_
#    define _DELUGE_UINT_H_
#    define __DELUGE_UINT_H__
#  endif
#endif


#ifdef __DELUGE_UINT_H__
#undef __DELUGE_UINT_H__


#include "deluge/opencl.h"


typedef struct
{
	uint64_t arr[4];
} uint256_t;

static inline void uint256_init_le64(uint256_t *restrict dst,
				     const uint64_t *restrict src)
{
	size_t i;

	for (i = 0; i < 4; i++)
		dst->arr[i] = src[i];
}


typedef struct
{
	uint64_t arr[5];
} uint320_t;


static inline void uint320_init_be64(uint320_t *restrict dst,
				     const uint64_t *restrict src)
{
	size_t i;

	for (i = 0; i < 5; i++)
		dst->arr[i] = src[4 - i];
}

static inline void uint320_dump_be64(uint64_t *restrict dst,
				     const uint320_t *restrict src)
{
	size_t i;

	for (i = 0; i < 5; i++)
		dst[i] = src->arr[4 - i];
}

static inline void uint320_init_le64(uint320_t *restrict dst,
				     const uint64_t *restrict src)
{
	size_t i;

	for (i = 0; i < 5; i++)
		dst->arr[i] = src[i];
}


void uint320_add(uint320_t *restrict dst, const uint320_t *restrict src);

void uint320_sum(local uint320_t *restrict arr, size_t n);


/* #if !defined (__OPENCL_VERSION__) */

/* #define UINT320_PRINTF_CODE          "%016lx%016lx%016lx%016lx%016lx" */

/* #define UINT320_PRINTF_ARG(_uint)    (_uint).arr[4], (_uint).arr[3],   \ */
/* 	                             (_uint).arr[2], (_uint).arr[1],   \ */
/* 	                             (_uint).arr[0] */

/* #endif  /\* !defined (__OPENCL_VERSION__) *\/ */


#endif
