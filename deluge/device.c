#include <deluge.h>
#include "deluge/deluge.h"
#include "deluge/device.h"
#include "deluge/error.h"
#include <stdio.h>


#define AVPROG_HIGHWAY   0x01


static void __debug(const char *errinfo,
		    const void *privinfo __attribute__ ((unused)),
		    size_t cb __attribute__ ((unused)),
		    void *user __attribute__ ((unused)))
{
	fprintf(stderr, "DEVICE ERROR: %s\n", errinfo);
}

int init_device(struct device *this, struct deluge *root, cl_device_id devid)
{
	cl_int clret;
	cl_ulong val;
	int err;

	clret = clGetDeviceInfo(devid, CL_DEVICE_TYPE, sizeof (this->devtype),
				&this->devtype, NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	clret = clGetDeviceInfo(devid, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof (val),
				&val, NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	} else {
		this->total_gmem = val;
	}

	clret = clGetDeviceInfo(devid, CL_DEVICE_LOCAL_MEM_SIZE, sizeof (val),
				&val, NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	} else {
		this->total_lmem = val;
	}

	this->ctx = clCreateContext(NULL, 1, &devid, __debug, this, &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	err = pthread_mutex_init(&this->lock, NULL);
	if (err != 0) {
		err = deluge_c_error();
		goto err_ctx;
	}

	this->root = root;
	this->devid = devid;
	this->used_gmem = 0;
	this->used_lmem = 0;
	this->avprogs = 0;

	return DELUGE_SUCCESS;
 err_ctx:
	clReleaseContext(this->ctx);
 err:
	return err;
}

void finlz_device(struct device *this)
{
	if (has_device_highway(this))
		finlz_highway_program(&this->highway);
	pthread_mutex_destroy(&this->lock);
	clReleaseContext(this->ctx);
}

size_t get_device_gmem(struct device *this)
{
	size_t ret;

	pthread_mutex_lock(&this->lock);
	ret = this->total_gmem - this->used_gmem;
	pthread_mutex_unlock(&this->lock);

	return ret;
}

size_t get_device_lmem(struct device *this)
{
	size_t ret;

	pthread_mutex_lock(&this->lock);
	ret = this->total_lmem - this->used_lmem;
	pthread_mutex_unlock(&this->lock);

	return ret;
}

int alloc_on_device(struct device *this, size_t gmem, size_t lmem)
{
	int ret;

	pthread_mutex_lock(&this->lock);

	if (this->total_gmem < (this->used_gmem + gmem)) {
		ret = DELUGE_OUT_OF_GMEM;
		goto out;
	}

	if (this->total_lmem < (this->used_lmem + lmem)) {
		ret = DELUGE_OUT_OF_LMEM;
		goto out;
	}

	this->used_gmem += gmem;
	this->used_lmem += lmem;

	ret = DELUGE_SUCCESS;
 out:
	pthread_mutex_unlock(&this->lock);

	return ret;
}

void free_on_device(struct device *this, size_t gmem, size_t lmem)
{
	pthread_mutex_lock(&this->lock);

	this->used_gmem -= gmem;
	this->used_lmem -= lmem;

	pthread_mutex_unlock(&this->lock);
}

int has_device_highway(const struct device *this)
{
	return ((this->avprogs & AVPROG_HIGHWAY) != 0);
}

static void set_device_highway(struct device *this)
{
	this->avprogs |= AVPROG_HIGHWAY;
}

int init_device_highway(struct device *this)
{
	int err;

	if (has_device_highway(this))
		return DELUGE_SUCCESS;

	err = init_highway_program(&this->highway, this);
	if (err != DELUGE_SUCCESS)
		goto err;

	set_device_highway(this);

	return DELUGE_SUCCESS;
 err:
	return err;
}
