#include <assert.h>
#include <deluge.h>
#include "deluge/deluge.h"
#include "deluge/device.h"
#include "deluge/error.h"
#include "deluge/opencl.h"
#include <pthread.h>


/* Deluge context of the current process */
static struct deluge __deluge;

/* Lock over process deluge context */
static pthread_mutex_t __deluge_lock = PTHREAD_MUTEX_INITIALIZER;

/* Reference count on the process deluge context */
static size_t __deluge_refcnt = 0;

/* Deluge context is initialized */
static int __deluge_inited = 0;


static int init_platforms(struct deluge *this)
{
	cl_uint nplid;
	cl_int clret;
	int err;

	clret = clGetPlatformIDs(0, NULL, &nplid);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	this->platforms = malloc(nplid * sizeof (*this->platforms));
	if (this->platforms == NULL) {
		err = deluge_c_error();
		goto err;
	}

	clret = clGetPlatformIDs(nplid, this->platforms, &nplid);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_platforms;
	}

	this->nplatform = nplid;

	return DELUGE_SUCCESS;
 err_platforms:
	free(this->platforms);
 err:
	return err;
}

static void finlz_platforms(struct deluge *this)
{
	size_t i;

	for (i = 0; i < this->nplatform; i++)
		clUnloadPlatformCompiler(this->platforms[i]);

	free(this->platforms);
}

static int init_devices(struct deluge *this)
{
	cl_uint i, ndevid, *ndevids;
	cl_device_id *devids;
	cl_int clret;
	int err;

	ndevids = malloc(this->nplatform * sizeof (*ndevids));
	if (ndevids == NULL) {
		err = deluge_c_error();
		goto err;
	}

	ndevid = 0;
	for (i = 0; i < this->nplatform; i++) {
		clret = clGetDeviceIDs(this->platforms[i], CL_DEVICE_TYPE_ALL,
				       0, NULL, &ndevids[i]);
		if (clret != CL_SUCCESS) {
			err = deluge_cl_error(clret);
			goto err_ndevids;
		}

		ndevid += ndevids[i];
	}

	devids = malloc(ndevid * sizeof (*devids));
	if (devids == NULL) {
		err = deluge_c_error();
		goto err_ndevids;
	}

	ndevid = 0;
	for (i = 0; i < this->nplatform; i++) {
		clret = clGetDeviceIDs(this->platforms[i], CL_DEVICE_TYPE_ALL,
				       ndevids[i], &devids[ndevid],
				       &ndevids[i]);
		if (clret != CL_SUCCESS) {
			err = deluge_cl_error(clret);
			goto err_devids;
		}

		ndevid += ndevids[i];
	}

	this->devices = malloc(ndevid * sizeof (*this->devices));
	if (this->devices == NULL) {
		err = deluge_c_error();
		goto err_devids;
	}

	for (i = 0; i < ndevid; i++) {
		err = init_device(&this->devices[i], this, devids[i]);
		if (err != DELUGE_SUCCESS)
			goto err_devices;
	}

	this->ndevice = ndevid;

	free(devids);
	free(ndevids);

	return DELUGE_SUCCESS;
 err_devices:
	while (i-- > 0)
		finlz_device(&this->devices[i]);
	free(this->devices);
 err_devids:
	free(devids);
 err_ndevids:
	free(ndevids);
 err:
	return err;
}

static void finlz_devices(struct deluge *this)
{
	size_t i;

	for (i = 0; i < this->ndevice; i++)
		finlz_device(&this->devices[i]);

	free(this->devices);
}

static int init_deluge(struct deluge *this)
{
	int err;

	err = init_platforms(this);
	if (err != DELUGE_SUCCESS)
		goto err;

	err = init_devices(this);
	if (err != DELUGE_SUCCESS)
		goto err_platforms;

	return DELUGE_SUCCESS;
 err_platforms:
	finlz_platforms(this);
 err:
	return err;
}

static void finlz_deluge(struct deluge *this)
{
	finlz_devices(this);
	finlz_platforms(this);
}


static int __deluge_init(void)
{
	int ret;

	if (__deluge_inited)
		return DELUGE_SUCCESS;

	ret = init_deluge(&__deluge);
	if (ret != DELUGE_SUCCESS)
		return ret;

	__deluge_inited = 1;
	assert(__deluge_refcnt == 0);

	return DELUGE_SUCCESS;
}

int deluge_init(void)
{
	int ret;

	pthread_mutex_lock(&__deluge_lock);
	ret = __deluge_init();
	pthread_mutex_unlock(&__deluge_lock);

	return ret;
}

void deluge_finalize(void)
{
	pthread_mutex_lock(&__deluge_lock);

	if ((__deluge_refcnt == 0) && (__deluge_inited)) {
		finlz_deluge(&__deluge);
		__deluge_inited = 0;
	}

	pthread_mutex_unlock(&__deluge_lock);
}


struct deluge *get_deluge(int *err)
{
	struct deluge *this;
	int _err;

	if (err == NULL)
		err = &_err;

	pthread_mutex_lock(&__deluge_lock);

	*err = __deluge_init();

	if (*err != DELUGE_SUCCESS) {
		this = NULL;
	} else {
		this = &__deluge;
		__deluge_refcnt += 1;
	}

	pthread_mutex_unlock(&__deluge_lock);

	return this;
}

void put_deluge(void)
{
	pthread_mutex_lock(&__deluge_lock);

	assert(__deluge_refcnt > 0);
	__deluge_refcnt -= 1;		

	pthread_mutex_unlock(&__deluge_lock);
}
