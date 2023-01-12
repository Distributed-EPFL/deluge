#include <deluge.h>
#include "deluge/atomic.h"
#include "deluge/deluge.h"
#include "deluge/device.h"
#include "deluge/error.h"
#include "deluge/highway.h"
#include <stdlib.h>


static ssize_t discover_platform_devices(struct deluge *this,
					 struct device *dest, size_t len,
					 cl_platform_id plid)
{
	cl_device_id *devids;
	cl_uint i, ndevid;
	cl_int clret;
	size_t done;
	int ret;

	devids = malloc(len * sizeof (*devids));
	if (devids == NULL) {
		deluge_c_error();
		goto err;
	}

	clret = clGetDeviceIDs(plid, CL_DEVICE_TYPE_ALL, len, devids, &ndevid);
	if (clret != CL_SUCCESS) {
		deluge_cl_error(clret);
		goto err_devids;
	}

	done = 0;
	for (i = 0; i < ndevid; i++) {
		ret = init_device(&dest[done], this, devids[i]);
		if (ret != DELUGE_SUCCESS)
			goto err_list;
		done += 1;
	}

	free(devids);

	return done;
 err_list:
	while (done-- > 0)
		finlz_device(&dest[done]);
 err_devids:
	free(devids);
 err:
	return -1;
}

static int discover_devices(struct deluge *this)
{
	cl_uint i, nplid, ndevid;
	cl_platform_id *plids;
	struct device *devs;
	size_t len, cap;
	cl_int clret;
	ssize_t ret;
	int err;

	clret = clGetPlatformIDs(0, NULL, &nplid);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	plids = malloc(nplid * sizeof (*plids));
	if (plids == NULL) {
		err = deluge_c_error();
		goto err;
	}

	clret = clGetPlatformIDs(nplid, plids, NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_plids;
	}

	cap = 0;
	for (i = 0; i < nplid; i++) {
		clret = clGetDeviceIDs(plids[i], CL_DEVICE_TYPE_ALL, 0, NULL,
				       &ndevid);
		if (clret != CL_SUCCESS) {
			err = deluge_cl_error(clret);
			goto err_plids;
		}

		cap += ndevid;
	}

	devs = malloc(cap * sizeof (*devs));
	if (devs == NULL) {
		err = deluge_c_error();
		goto err_plids;
	}

	len = 0;
	for (i = 0; i < nplid; i++) {
		ret = discover_platform_devices(this, devs+len, cap, plids[i]);

		if (ret < 0) {
			err = DELUGE_FAILURE;
			goto err_list;
		} else {
			len += (size_t) ret;
			cap -= (size_t) ret;
		}
	}

	this->devices = realloc(devs, len * sizeof (*devs));
	this->ndevice = len;

	free(plids);

	return DELUGE_SUCCESS;
 err_list:
	while (len-- > 0)
		finlz_device(&devs[len]);
	free(devs);
 err_plids:
	free(plids);
 err:
	return err;
}

static int init_deluge(struct deluge *this)
{
	int err;

	err = discover_devices(this);
	if (err != DELUGE_SUCCESS)
		goto err_discover;

	atomic_store_uint64(&this->refcnt, 1);

	return DELUGE_SUCCESS;
 err_discover:
	return err;
}

static void finlz_deluge(struct deluge *this)
{
	size_t i;

	for (i = 0; i < this->ndevice; i++)
		finlz_device(&this->devices[i]);

	free(this->devices);
}

struct deluge *retain_deluge(struct deluge *this)
{
	atomic_add_uint64(&this->refcnt, 1);
	return this;
}

void release_deluge(struct deluge *this)
{
	if (atomic_sub_uint64(&this->refcnt, 1) > 0)
		return;
	finlz_deluge(this);
	free(this);
}


int deluge_create(deluge_t *deluge)
{
	struct deluge *this;
	int err;

	this = malloc(sizeof (struct deluge));
	if (this == NULL) {
		err = deluge_c_error();
		goto err;
	}

	err = init_deluge(this);
	if (err != DELUGE_SUCCESS)
		goto err_this;

	*deluge = this;

	return DELUGE_SUCCESS;
 err_this:
	free(this);
 err:
	*deluge = NULL;   /* make gcc happy */
	return err;
}

void deluge_destroy(deluge_t deluge)
{
	release_deluge(deluge);
}
