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


/*
 * Host compatibility for device builtins
 */

#if !defined (__OPENCL_VERSION__)


#ifndef CL_TARGET_OPENCL_VERSION
#  define CL_TARGET_OPENCL_VERSION 300
#endif


#include <CL/cl.h>
#include <stddef.h>     /* size_t */
#include <stdio.h>      /* printf() */


const char *opencl_errstr(cl_int ret);


#define kernel
#define global
#define constant
#define local
#define private


#endif  /* !defined (__OPENCL_VERSION__) */



/*
 * Host/device compatibility for <stdint.h>
 */

#if !defined (__OPENCL_VERSION__)

#include <stdint.h>

#else  /* defined (__OPENCL_VERSION__) */

typedef char  int8_t;
typedef short int16_t;
typedef int   int32_t;
typedef long  int64_t;

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

#endif  /* defined (__OPENCL_VERSION__) */


/*
 * Host/device compatibility for <endian.h>
 */

#if !defined (__OPENCL_VERSION__)

#include <endian.h>

#else  /* defined (__OPENCL_VERSION__) */

#define __bswap_16(x)				\
	((__uint16_t) ((((x) >> 8) & 0xff)	\
		       | (((x) & 0xff) << 8)))

#define __bswap_32(x)				\
	((((x) & 0xff000000u) >> 24)		\
	 | (((x) & 0x00ff0000u) >> 8)		\
	 | (((x) & 0x0000ff00u) << 8)		\
	 | (((x) & 0x000000ffu) << 24))

#define __bswap_64(x)				      \
	((((x) & 0xff00000000000000ull) >> 56)        \
	 | (((x) & 0x00ff000000000000ull) >> 40)      \
	 | (((x) & 0x0000ff0000000000ull) >> 24)      \
	 | (((x) & 0x000000ff00000000ull) >> 8)       \
	 | (((x) & 0x00000000ff000000ull) << 8)       \
	 | (((x) & 0x0000000000ff0000ull) << 24)      \
	 | (((x) & 0x000000000000ff00ull) << 40)      \
	 | (((x) & 0x00000000000000ffull) << 56))

#if (__BYTE_ORDER == __LITTLE_ENDIAN)
# define htobe16(x) __bswap_16(x)
# define htole16(x) x
# define be16toh(x) __bswap_16(x)
# define le16toh(x) x
# define htobe32(x) __bswap_32(x)
# define htole32(x) x
# define be32toh(x) __bswap_32(x)
# define le32toh(x) x
# define htobe64(x) __bswap_64(x)
# define htole64(x) x
# define be64toh(x) __bswap_64(x)
# define le64toh(x) x
#else
# define htobe16(x) x
# define htole16(x) __bswap_16(x)
# define be16toh(x) x
# define le16toh(x) __bswap_16(x)
# define htobe32(x) x
# define htole32(x) __bswap_32(x)
# define be32toh(x) x
# define le32toh(x) __bswap_32(x)
# define htobe64(x) x
# define htole64(x) __bswap_64(x)
# define be64toh(x) x
# define le64toh(x) __bswap_64(x)
#endif

#ifdef __ENDIAN_LITTLE__
# define dtobe16(x) __bswap_16(x)
# define dtole16(x) x
# define be16tod(x) __bswap_16(x)
# define le16tod(x) x
# define dtobe32(x) __bswap_32(x)
# define dtole32(x) x
# define be32tod(x) __bswap_32(x)
# define le32tod(x) x
# define dtobe64(x) __bswap_64(x)
# define dtole64(x) x
# define be64tod(x) __bswap_64(x)
# define le64tod(x) x
#else
# define dtobe16(x) x
# define dtole16(x) __bswap_16(x)
# define be16toh(x) x
# define le16toh(x) __bswap_16(x)
# define dtobe32(x) x
# define dtole32(x) __bswap_32(x)
# define be32toh(x) x
# define le32toh(x) __bswap_32(x)
# define dtobe64(x) x
# define dtole64(x) __bswap_64(x)
# define be64tod(x) x
# define le64tod(x) __bswap_64(x)
#endif

#endif  /* defined (__OPENCL_VERSION__) */


/*
 * Additional endianness conversion between host and device
 */

#if defined (__OPENCL_VERSION__)

#if (__BYTE_ORDER == __LITTLE_ENDIAN)            /* host is little endian */
# ifdef __ENDIAN_LITTLE__                        /*  device is little endian */
#  define htod16(x) x
#  define dtoh16(x) x
#  define htod32(x) x
#  define dtoh32(x) x
#  define htod64(x) x
#  define dtoh64(x) x
# else                                           /*  device is big endian */
#  define htod16(x) __bswap_16(x)
#  define dtoh16(x) __bswap_16(x)
#  define htod32(x) __bswap_32(x)
#  define dtoh32(x) __bswap_32(x)
#  define htod64(x) __bswap_64(x)
#  define dtoh64(x) __bswap_64(x)
# endif
#else                                            /* host is big endian */
# ifdef __ENDIAN_LITTLE__                        /*  device is little endian */
#  define htod16(x) __bswap_16(x)
#  define dtoh16(x) __bswap_16(x)
#  define htod32(x) __bswap_32(x)
#  define dtoh32(x) __bswap_32(x)
#  define htod64(x) __bswap_64(x)
#  define dtoh64(x) __bswap_64(x)
# else                                           /*  device is big endian */
#  define htod16(x) x
#  define dtoh16(x) x
#  define htod32(x) x
#  define dtoh32(x) x
#  define htod64(x) x
#  define dtoh64(x) x
# endif
#endif

#endif  /* defined (__OPENCL_VERSION__) */


#endif  /* __DELUGE_OPENCL_H__ */
