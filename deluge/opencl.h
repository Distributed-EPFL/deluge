#if defined (__OPENCL_VERSION__)
#  ifndef _DELUGE_OPENCL_H_BIN_
#    define _DELUGE_OPENCL_H_BIN_
#    define __DELUGE_OPENCL_H__
#  endif
#else
#  ifndef _DELUGE_OPENCL_H_
#    define _DELUGE_OPENCL_H_
#    define __DELUGE_OPENCL_H__
#  endif
#endif


#ifdef __DELUGE_OPENCL_H__
#undef __DELUGE_OPENCL_H__


#if defined (__OPENCL_VERSION__)


typedef char  int8_t;
typedef short int16_t;
typedef int   int32_t;
typedef long  int64_t;

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;


#define __bswap_64(x)			      \
	((((x) & 0xff00000000000000ull) >> 56)        \
	 | (((x) & 0x00ff000000000000ull) >> 40)      \
	 | (((x) & 0x0000ff0000000000ull) >> 24)      \
	 | (((x) & 0x000000ff00000000ull) >> 8)       \
	 | (((x) & 0x00000000ff000000ull) << 8)       \
	 | (((x) & 0x0000000000ff0000ull) << 24)      \
	 | (((x) & 0x000000000000ff00ull) << 40)      \
	 | (((x) & 0x00000000000000ffull) << 56))


#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)  /* host is little endian */
#  ifdef __ENDIAN_LITTLE__                       /*  device is little endian */
#    define htod64(_host_64)   (_host_64)
#    define dtoh64(_dev_64)    (_dev_64)
#  else                                          /*  device is big endian */
#    define htod64(_host_64)   __bswap_64(_host_64)
#    define dtoh64(_host_64)   __bswap_64(_host_64)
#  endif
#else                                            /* host is big endian */
#  ifdef __ENDIAN_LITTLE__                       /*  device is little endian */
#    define htod64(_host_64)   __bswap_64(_host_64)
#    define dtoh64(_host_64)   __bswap_64(_host_64)
#  else                                          /*  device is big endian */
#    define htod64(_host_64)   (_host_64)
#    define dtoh64(_dev_64)    (_dev_64)
#  endif
#endif


#else  /* !defined (__OPENCL_VERSION__) */


#ifndef CL_TARGET_OPENCL_VERSION
#  define CL_TARGET_OPENCL_VERSION 300
#endif


#include <CL/cl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>


const char *opencl_errstr(cl_int ret);


#define kernel
#define global
#define constant
#define local
#define private


#endif  /* !defined (__OPENCL_VERSION__) */


#endif
