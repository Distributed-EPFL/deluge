#include <deluge.h>
#include "deluge/blake3.h"
#include "deluge/compile.h"
#include "deluge/device.h"
#include "deluge/error.h"
#include "deluge/hashsum64.h"
#include "deluge/util.h"
#include <string.h>


struct hs64b3
{
	struct hashsum64  super;     /* generic hashsum64 logic */
	blake3_t          digest;    /* initial digest state */
};


extern const char _binary_deluge_blake3_cl_start[];
extern const char _binary_deluge_blake3_cl_end[];

extern const char _binary_deluge_blake3_h_start[];
extern const char _binary_deluge_blake3_h_end[];

extern const char _binary_deluge_opencl_h_start[];
extern const char _binary_deluge_opencl_h_end[];

extern const char _binary_deluge_uint_cl_start[];
extern const char _binary_deluge_uint_cl_end[];

extern const char _binary_deluge_uint_h_start[];
extern const char _binary_deluge_uint_h_end[];


static const struct text_source __headers[] = {
	{
		"deluge/blake3.h",
		_binary_deluge_blake3_h_start,
		_binary_deluge_blake3_h_end
	},
	{
		"deluge/opencl.h",
		_binary_deluge_opencl_h_start,
		_binary_deluge_opencl_h_end
	},
	{
		"deluge/uint.h",
		_binary_deluge_uint_h_start,
		_binary_deluge_uint_h_end
	}
};

static const struct text_source __sources[] = {
	{
		"deluge/blake3.cl",
		_binary_deluge_blake3_cl_start,
		_binary_deluge_blake3_cl_end
	},
	{
		"deluge/uint.cl",
		_binary_deluge_uint_cl_start,
		_binary_deluge_uint_cl_end
	}
};


static void finlz_hs64b3(struct hs64b3 *this);


static int __prepare(struct hashsum64 *this __attribute__ ((unused)),
		     cl_kernel *entry, struct device *dev)
{
	cl_program *lib;
	cl_int clret;
	int err;

	lib = get_device_blake3(dev, &err);
	if (err != DELUGE_SUCCESS)
		goto err;

	*entry = clCreateKernel(*lib, "hashsum64", &clret);
	if (clret != CL_SUCCESS)
		goto err;

	return DELUGE_SUCCESS;
 err:
	return err;
}

static int __setup(struct hashsum64 *_this, void *dest, size_t *len,
		   struct device *dev)
{
	struct hs64b3 *this = container_of(_this, struct hs64b3, super);
	blake3_t *scratch;
	cl_int clret;
	cl_bool le;
	size_t i;
	int err;

	if (dest == NULL) {
		*len = sizeof (this->digest);
		return DELUGE_SUCCESS;
	}

	if ((len == NULL) || (*len < sizeof (this->digest))) {
		return DELUGE_FAILURE;
	}

	clret = clGetDeviceInfo(dev->id, CL_DEVICE_ENDIAN_LITTLE, sizeof (le),
				&le, NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	scratch = dest;

	if (le)
		for (i = 0; i < ARRAY_SIZE(this->digest.state); i++)
			scratch->state[i] = htole32(this->digest.state[i]);
	else
		for (i = 0; i < ARRAY_SIZE(this->digest.state); i++)
			scratch->state[i] = htobe32(this->digest.state[i]);

	*len = sizeof (this->digest);

	return DELUGE_SUCCESS;
 err:
	return err;
}

static void __destroy(struct hashsum64 *_this)
{
	struct hs64b3 *this = container_of(_this, struct hs64b3, super);

	finlz_hs64b3(this);
	free(this);
}


static void init_blake3_state(blake3_t *this, const void *key)
{
	memcpy(&this->state, key, DELUGE_BLAKE3_KEYLEN);

	this->state[8] = 0x6A09E667ul;
	this->state[9] = 0xBB67AE85ul;
	this->state[10] = 0x3C6EF372ul;
	this->state[11] = 0xA54FF53Aul;
	this->state[12] = 0;
	this->state[13] = 0;
	this->state[14] = sizeof (uint64_t);
	this->state[15] = 0x1b;

	blake3_g(this->state, 1, 5, 9, 13, 0, 0);
	blake3_g(this->state, 2, 6, 10, 14, 0, 0);
	blake3_g(this->state, 3, 7, 11, 15, 0, 0);
}

int init_blake3_program(cl_program *text, struct device *dev)
{
	return init_text(text, dev, __headers, ARRAY_SIZE(__headers),
			 __sources, ARRAY_SIZE(__sources));
}

static int init_hs64b3_devices(struct hs64b3 *this)
{
	struct deluge *root = this->super.super.root;
	size_t i;
	int err;

	if (root->ndevice == 0) {
		err = DELUGE_NODEV;
		goto err;
	}

	for (i = 0; i < root->ndevice; i++) {
		err = init_device_blake3(&root->devices[i]);
		if (err != DELUGE_SUCCESS)
			goto err;
	}

	return DELUGE_SUCCESS;
 err:
	return err;
}

static int init_hs64b3(struct hs64b3 *this, const void *key)
{
	static struct hashsum64_vtable vtable = {
		.prepare = __prepare,
		.setup = __setup,
		.destroy = __destroy
	};
	int err;

	err = init_hashsum64(&this->super, &vtable);
	if (err != DELUGE_SUCCESS)
		goto err;

	err = init_hs64b3_devices(this);
	if (err != DELUGE_SUCCESS)
		goto err_hashsum64;

	init_blake3_state(&this->digest, key);

	return DELUGE_SUCCESS;
 err_hashsum64:
	finlz_hashsum64(&this->super);
 err:
	return err;
}

static void finlz_hs64b3(struct hs64b3 *this)
{
	finlz_hashsum64(&this->super);
}


int deluge_hashsum64_blake3_create(deluge_dispatch_t *restrict _this,
				   const void *key)
{
	struct hs64b3 *this;
	int err;

	this = malloc(sizeof (*this));
	if (this == NULL) {
		err = deluge_c_error();
		goto err;
	}

	err = init_hs64b3(this, key);
	if (err != DELUGE_SUCCESS)
		goto err_this;

	*_this = &this->super.super;

	return DELUGE_SUCCESS;
 err_this:
	free(this);
 err:
	*_this = NULL;  /* make gcc happy */
	return err;
}
