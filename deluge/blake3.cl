#include "deluge/blake3.h"
#include "deluge/uint.h"


static uint32_t load32(const void *src) {
  const uint8_t *p = (const uint8_t *)src;
  return ((uint32_t)(p[0]) << 0) | ((uint32_t)(p[1]) << 8) |
         ((uint32_t)(p[2]) << 16) | ((uint32_t)(p[3]) << 24);
}

static const uint8_t MSG_SCHEDULE[6][12] = {
	{2, 2, 0, 2, 1, 2, 2, 2, 2, 2, 2, 2},
	{2, 2, 2, 2, 2, 2, 2, 0, 2, 2, 2, 1},
	{2, 2, 2, 2, 2, 0, 2, 2, 2, 2, 1, 2},
	{2, 2, 2, 2, 2, 2, 2, 2, 0, 1, 2, 2},
	{2, 2, 2, 1, 2, 2, 0, 2, 2, 2, 2, 2},
	{0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
};

static void round_fn(uint32_t *state, const uint32_t *msg, size_t round) {
	const uint8_t *schedule = MSG_SCHEDULE[round];

	// Mix the columns.
	blake3_g(state, 0, 4, 8, 12, 0, 0);
	blake3_g(state, 1, 5, 9, 13, 0, msg[schedule[0]]);
	blake3_g(state, 2, 6, 10, 14, msg[schedule[1]], msg[schedule[2]]);
	blake3_g(state, 3, 7, 11, 15, 0, msg[schedule[3]]);

	// Mix the rows.
	blake3_g(state, 0, 5, 10, 15, msg[schedule[4]], msg[schedule[5]]);
	blake3_g(state, 1, 6, 11, 12, msg[schedule[6]], msg[schedule[7]]);
	blake3_g(state, 2, 7, 8, 13, msg[schedule[8]], msg[schedule[9]]);
	blake3_g(state, 3, 4, 9, 14, msg[schedule[10]], msg[schedule[11]]);
}

static void compress_pre(uint32_t *state, uint32_t block_words[3])
{
	blake3_g(state, 0, 4, 8, 12, block_words[0], block_words[1]);
	blake3_g(state, 0, 5, 10, 15, 0, 0);
	blake3_g(state, 1, 6, 11, 12, 0, 0);
	blake3_g(state, 2, 7, 8, 13, 0, 0);
	blake3_g(state, 3, 4, 9, 14, 0, 0);

	round_fn(state, block_words, 0);
	round_fn(state, block_words, 1);
	round_fn(state, block_words, 2);
	round_fn(state, block_words, 3);
	round_fn(state, block_words, 4);
	round_fn(state, block_words, 5);
}

static void hash(blake3_t *self, uint64_t elem, uint256_t *out)
{
	uint32_t block_words[3];
	size_t i;

	block_words[0] = load32((uint8_t *) &elem);
	block_words[1] = load32(((uint8_t *) &elem) + 4);
	block_words[2] = 0;

	compress_pre(self->state, block_words);

	uint256_store_le32(out, 7 * 4, self->state[0] ^ self->state[8]);
	uint256_store_le32(out, 6 * 4, self->state[1] ^ self->state[9]);
	uint256_store_le32(out, 5 * 4, self->state[2] ^ self->state[10]);
	uint256_store_le32(out, 4 * 4, self->state[3] ^ self->state[11]);
	uint256_store_le32(out, 3 * 4, self->state[4] ^ self->state[12]);
	uint256_store_le32(out, 2 * 4, self->state[5] ^ self->state[13]);
	uint256_store_le32(out, 1 * 4, self->state[6] ^ self->state[14]);
	uint256_store_le32(out, 0 * 4, self->state[7] ^ self->state[15]);
}

static void reduce(size_t nelem, local uint320_t *restrict mem,
		   private const uint256_t *restrict val)
{
	size_t wg_size = get_local_size(0);
	size_t wg_elem = get_global_id(0) - get_local_id(0) + wg_size;

	if (wg_elem > nelem)
		nelem = wg_size - (wg_elem - nelem);
	else
		nelem = wg_size;

	if (get_local_id(0) < nelem)
		uint320_init_256(&mem[get_local_id(0)], val);

	uint320_sum(mem, nelem);
}

kernel void hashsum64(uint64_t nelem, local uint320_t *mem,
		      global const uint64_t *elems, global uint320_t *results,
		      constant const blake3_t *restrict initial)
{
	private uint256_t digest;
	private uint64_t elem;
	private blake3_t st;
	size_t gid;

	gid = get_global_id(0);

	/* set digest from precomputed initial state */
	st = *initial;

	/* compute highway hash - store result in `uint256_t` */
	/* TODO: make sure elems[gid] is interpreted big endian */
	if (gid < nelem)
		hash(&st, htobe64(elems[gid]), &digest);

	reduce(nelem, mem, &digest);

 	if (get_local_id(0) == 0) {
		uint320_dtoh(mem);
		results[gid / get_local_size(0)] = *mem;
	}
}
