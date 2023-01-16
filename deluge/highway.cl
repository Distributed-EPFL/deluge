#include "deluge/highway.h"
#include "deluge/uint.h"


static void zipper_merge_and_add(const uint64_t v1, const uint64_t v0,
                                 uint64_t *restrict add1,
				 uint64_t *restrict add0)
{
        *add0 += (((v0 & 0xff000000ull) | (v1 & 0xff00000000ull)) >> 24) |
                (((v0 & 0xff0000000000ull) |
                  (v1 & 0xff000000000000ull)) >> 16) |
                (v0 & 0xff0000ull) | ((v0 & 0xff00ull) << 32) |
                ((v1 & 0xff00000000000000ull) >> 8) | (v0 << 56);
        *add1 += (((v1 & 0xff000000ull) | (v0 & 0xff00000000ull)) >> 24) |
                (v1 & 0xff0000ull) | ((v1 & 0xff0000000000ull) >> 16) |
                ((v1 & 0xff00ull) << 24) |
                ((v0 & 0xff000000000000ull) >> 8) |
                ((v1 & 0xffull) << 48) | (v0 & 0xff00000000000000ull);
}

static void update(const uint64_t lanes[4], highway_t *restrict st)
{
        int i;

        for (i = 0; i < 4; ++i) {
                st->v1[i] += st->mul0[i] + lanes[i];
                st->mul0[i] ^= (st->v1[i] & 0xffffffff) * (st->v0[i] >> 32);
                st->v0[i] += st->mul1[i];
                st->mul1[i] ^= (st->v0[i] & 0xffffffff) * (st->v1[i] >> 32);
        }

        zipper_merge_and_add(st->v1[1], st->v1[0], &st->v0[1], &st->v0[0]);
        zipper_merge_and_add(st->v1[3], st->v1[2], &st->v0[3], &st->v0[2]);
        zipper_merge_and_add(st->v0[1], st->v0[0], &st->v1[1], &st->v1[0]);
        zipper_merge_and_add(st->v0[3], st->v0[2], &st->v1[3], &st->v1[2]);
}

static void permute(generic const uint64_t v[4], uint64_t permuted[4])
{
        permuted[0] = (v[2] >> 32) | (v[2] << 32);
        permuted[1] = (v[3] >> 32) | (v[3] << 32);
        permuted[2] = (v[0] >> 32) | (v[0] << 32);
        permuted[3] = (v[1] >> 32) | (v[1] << 32);
}

static void permute_and_update(highway_t *restrict st)
{
        uint64_t permuted[4];

        permute(st->v0, permuted);

        update(permuted, st);
}

static void modular_reduction(uint64_t a3_unmasked, uint64_t a2, uint64_t a1,
                              uint64_t a0,
                              uint64_t *restrict m1, uint64_t *restrict m0)
{
        uint64_t a3 = a3_unmasked & 0x3fffffffffffffffull;
        *m1 = a1 ^ ((a3 << 1) | (a2 >> 63)) ^ ((a3 << 2) | (a2 >> 62));
        *m0 = a0 ^ (a2 << 1) ^ (a2 << 2);
}

static void finalize_256(highway_t *restrict st, generic uint64_t hash[4])
{
        int i;

        for (i = 0; i < 10; i++)
                permute_and_update(st);

        modular_reduction(st->v1[1] + st->mul1[1],
                          st->v1[0] + st->mul1[0],
                          st->v0[1] + st->mul0[1],
                          st->v0[0] + st->mul0[0],
                          &hash[2], &hash[3]);
        modular_reduction(st->v1[3] + st->mul1[3],
                          st->v1[2] + st->mul1[2],
                          st->v0[3] + st->mul0[3],
                          st->v0[2] + st->mul0[2],
                          &hash[0], &hash[1]);
}

static void hash(highway_t *restrict st, uint64_t *restrict h, uint64_t d)
{
        uint64_t lanes[4];

	lanes[0] = d;
	lanes[1] = 0;
	lanes[2] = 0;
	lanes[3] = 0;

        update(lanes, st);

	finalize_256(st, h);
}



static void reduction_320(size_t n, local uint320_t *restrict mem,
			  private const uint256_t *restrict val)
{
	size_t last_group = get_num_groups(0) - 1;
	size_t group_size = get_local_size(0);

	uint320_init_256(&mem[get_local_id(0)], val);

	if (get_group_id(0) == last_group)
		n = n - last_group * group_size;
	else
		n = group_size;

	uint320_sum(mem, n);
}

kernel void hash_sum(uint64_t n, global const uint64_t *gin,
		     constant const highway_t *restrict initial_st,
		     global uint320_t *gout, local uint320_t *lmem)
{
	private uint256_t digest;
	private uint64_t elem;
	private highway_t st;
	size_t i, gid;

	gid = get_global_id(0);
	if (gid >= n)
		return;

	/* compute highway hash */
	st = *initial_st;
	hash(&st, &digest, gin[gid]);

	reduction_320(n, lmem, &digest);

	if (get_local_id(0) != 0)
		return;

	for (i = 0; i < 5; i++) {
		elem = uint320_cast_le64(&lmem[0])[i];
		uint320_cast_le64(&gout[get_group_id(0)])[i] = elem;
	}
}
