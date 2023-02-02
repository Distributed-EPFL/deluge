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


/*
 * 256 bits unsigned integer.
 * Internal representation is private.
 */
typedef struct
{
	uint64_t arr[4];     /* `arr[0]` contains the least significant bits */
} uint256_t;


/*
 * Return the value of the byte at the given `off`set where `off = 0` means the
 * least significant byte.
 */
static inline uint8_t uint256_load_8(const uint256_t *restrict this,
				     size_t off)
{
	const size_t slot = off / sizeof (uint64_t);
	const size_t shift = 8 * (off % sizeof (uint64_t));

	return ((this->arr[slot] >> shift) & 0xff);
}

/*
 * Set the value of the byte at the given `off`set where `off = 0` means the
 * least significant byte.
 */
static inline void uint256_store_8(uint256_t *restrict this, size_t off,
				   uint8_t val)
{
	const size_t slot = off / sizeof (uint64_t);
	const size_t shift = 8 * (off % sizeof (uint64_t));
	const uint64_t mask = 0xfful << shift;

	this->arr[slot] &= ~mask;
	this->arr[slot] |= (((uint64_t) val) << shift);
}

/*
 * Store the given little endian 32 bits `val`ue at the given `off`set.
 * Using `off = 0` means to set the 4 least significant bytes.
 * Using `off = 28` means to set the 4 most significant bytes.
 */
static inline void uint256_store_le32(uint256_t *restrict this, size_t off,
				      uint32_t val)
{
	uint256_store_8(this, off + 3,  val        & 0xff);
	uint256_store_8(this, off + 2, (val >>  8) & 0xff);
	uint256_store_8(this, off + 1, (val >> 16) & 0xff);
	uint256_store_8(this, off,     (val >> 24) & 0xff);
}


/*
 * Initialize the value from a big endian array of bytes.
 */
static inline void uint256_init_be(uint256_t *restrict dst,
				   const uint8_t *restrict src)
{
	static const size_t len = 4 * sizeof (uint64_t);
	size_t i;

	for (i = 0; i < len; i++)
		uint256_store_8(dst, i, src[len - 1 - i]);
}

/*
 * Convert the given `uint256_t` from host internal representation to device
 * internal representation.
 */
#define uint256_htod(_this)   do {} while (0)

/*
 * Convert the given `uint256_t` from device internal representation to host
 * internal representation.
 */
#define uint256_dtoh(_this)   do {} while (0)


/*
 * 320 bits unsigned integer.
 * Internal representation is private.
 */
typedef struct
{
	uint64_t arr[5];     /* `arr[0]` contains the least significant bits */
} uint320_t;


/*
 * Return the value of the byte at the given `off`set where `off = 0` means the
 * least significant byte.
 */
static inline uint8_t uint320_get8(const uint320_t *restrict this, size_t off)
{
	const size_t slot = off / sizeof (uint64_t);
	const size_t shift = 8 * (off % sizeof (uint64_t));

	return ((this->arr[slot] >> shift) & 0xff);
}

/*
 * Set the value of the byte at the given `off`set where `off = 0` means the
 * least significant byte.
 */
static inline void uint320_set8(uint320_t *restrict this, size_t off,
				uint8_t val)
{
	const size_t slot = off / sizeof (uint64_t);
	const size_t shift = 8 * (off % sizeof (uint64_t));
	const uint64_t mask = 0xfful << shift;

	this->arr[slot] &= ~mask;
	this->arr[slot] |= (((uint64_t) val) << shift);
}


/*
 * Initialize the value from a big endian array of bytes.
 */
static inline void uint320_init_be(uint320_t *restrict dst,
				   const uint8_t *restrict src)
{
	static const size_t len = 5 * sizeof (uint64_t);
	size_t i;

	for (i = 0; i < len; i++)
		uint320_set8(dst, i, src[len - 1 - i]);
}

/*
 * Initialize the value from an upcast `uint256_t`.
 * The 64 most significant bits are set to `0`.
 */
static inline void uint320_init_256(uint320_t *restrict dst,
				    const uint256_t *restrict src)
{
	size_t i;

	for (i = 0; i < 4; i++)
		dst->arr[i] = src->arr[i];

	dst->arr[4] = 0;
}

/*
 * Initialize the value from a big endian array of bytes.
 */
static inline void uint320_dump_be(uint8_t *restrict dst,
				   const uint320_t *restrict src)
{
	static const size_t len = 5 * sizeof (uint64_t);
	size_t i;

	for (i = 0; i < len; i++)
		dst[len - 1 - i] = uint320_get8(src, i);
}


#if defined (__OPENCL_VERSION__)

/*
 * Convert the given `uint320_t` from host internal representation to device
 * internal representation.
 */
static inline void uint320_htod(uint320_t *restrict this)
{
	size_t i;

	for (i = 0; i < 5; i++)
		this->arr[i] = htod64(this->arr[i]);
}

/*
 * Convert the given `uint320_t` from device internal representation to host
 * internal representation.
 */
static inline void uint320_dtoh(uint320_t *restrict this)
{
	size_t i;

	for (i = 0; i < 5; i++)
		this->arr[i] = dtoh64(this->arr[i]);
}

#endif  /* defined (__OPENCL_VERSION__) */


/*
 * Add two `uint320_t` together and store the result in the first operand.
 */
void uint320_add(uint320_t *restrict dst, const uint320_t *restrict src);

/*
 * Compute the sum of `n` `uint320_t` elements stored in the given `arr`.
 */
void uint320_sum(local uint320_t *restrict arr, size_t n);


#endif
