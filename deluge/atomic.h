#ifndef _DELUGE_ATOMIC_H_
#define _DELUGE_ATOMIC_H_


#include <stdint.h>


typedef struct
{
	uint64_t val;
} atomic_uint64_t;

static inline void atomic_store_uint64(atomic_uint64_t *dest, uint64_t val)
{
	__atomic_store(&dest->val, &val, __ATOMIC_SEQ_CST);
}

static inline uint64_t atomic_load_uint64(atomic_uint64_t *dest)
{
	uint64_t ret;
	__atomic_load(&dest->val, &ret, __ATOMIC_SEQ_CST);
	return ret;
}

static inline uint64_t atomic_add_uint64(atomic_uint64_t *dest, uint64_t val)
{
	return __atomic_add_fetch(&dest->val, val, __ATOMIC_SEQ_CST);
}

static inline uint64_t atomic_sub_uint64(atomic_uint64_t *dest, uint64_t val)
{
	return __atomic_sub_fetch(&dest->val, val, __ATOMIC_SEQ_CST);
}


#endif
