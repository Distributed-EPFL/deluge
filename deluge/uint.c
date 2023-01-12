#include "deluge/uint.h"
#include <stddef.h>


void uint320_add(uint320_t *restrict dst, const uint320_t *restrict src)
{
	uint64_t carry = 0;
	size_t i;

	for (i = 0; i < 5; i++) {
		carry = __builtin_add_overflow(dst->arr[i], carry,
					       &dst->arr[i]);
		carry += __builtin_add_overflow(dst->arr[i], src->arr[i],
						&dst->arr[i]);
	}
}

void uint320_sum(uint320_t *restrict arr, size_t n)
{
	size_t i;

	for (i = 1; i < n; i++)
		uint320_add(&arr[0], &arr[i]);
}
