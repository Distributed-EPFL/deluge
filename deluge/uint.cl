#include "deluge/uint.h"


#define ARRAY_SIZE(_arr)  (sizeof (_arr) / sizeof (*(_arr)))
#define ARR_SIZE          ARRAY_SIZE(((uint320_t *) NULL)->arr)


void uint320_add(uint320_t *restrict dst, const uint320_t *restrict src)
{
	uint64_t tmp, carry = 0;
	size_t i;

	for (i = 0; i < ARR_SIZE; i++) {
		tmp = dst->arr[i] + src->arr[i] + carry;
		if (carry)
			carry = rhadd(dst->arr[i], src->arr[i]);
		else
			carry = hadd(dst->arr[i], src->arr[i]);
		dst->arr[i] = tmp;
		carry = carry >> 63;
	}
}

static void uint320_add_local(local uint320_t *restrict arr, size_t n,
			      size_t stride)
{
	size_t cidx, pidx;

	cidx = get_local_id(0);
	pidx = cidx + stride;

	if (pidx >= n)
		return;

	uint320_add(&arr[cidx], &arr[pidx]);
}

void uint320_sum(local uint320_t *restrict arr, size_t n)
{
	size_t stride;

	stride = (1 << (64 - clz(((uint64_t) n) - 1))) >> 1;

	while (n > 1) {
		barrier(CLK_LOCAL_MEM_FENCE);
		uint320_add_local(arr, n, stride);
		n = stride;
		stride /= 2;
	}
}
