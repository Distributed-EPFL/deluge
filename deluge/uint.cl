#include "deluge/uint.h"


void uint320_add(uint320_t *restrict dst, const uint320_t *restrict src)
{
	uint64_t tmp, carry = 0;
	size_t i;

	for (i = 0; i < 5; i++) {
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

	barrier(CLK_LOCAL_MEM_FENCE);
}

kernel void sum320(uint64_t n, local uint320_t *mem, global uint320_t *elems)
{
	size_t last_group = get_num_groups(0) - 1;
	size_t group_size = get_local_size(0);

	if (get_group_id(0) == last_group)
		n = n - last_group * group_size;
	else
		n = group_size;

	if (get_local_id(0) < n)
		mem[get_local_id(0)] = elems[get_global_id(0)];

	uint320_sum(mem, n);

	if (get_local_id(0) == 0)
		elems[get_group_id(0)] = mem[0];
}
