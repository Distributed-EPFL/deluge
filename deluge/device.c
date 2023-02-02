#include <deluge.h>
#include "deluge/blake3.h"
#include "deluge/device.h"
#include "deluge/error.h"
#include <pthread.h>


#define PROGRAM_BLAKE3  0x1


#ifndef NDEBUG

#include <stdio.h>

static void __debug_notify(const char *errinfo,
			   const void *private_info __attribute__ ((unused)),
			   size_t cb __attribute__ ((unused)),
			   void *user_data __attribute__ ((unused)))
{
	fprintf(stderr, "deluge log: %s\n", errinfo);
}

static void (*__notify_cb)(const char *, const void *, size_t, void *) =
	__debug_notify;

#else

static void (*__notify_cb)(const char *, const void *, size_t, void *) = NULL;

#endif


int init_device(struct device *this, struct deluge *root, cl_device_id id)
{
	cl_int clret;
	int err;

	this->ctx = clCreateContext(NULL, 1, &id, __notify_cb, NULL, &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	err = pthread_mutex_init(&this->lock, NULL);
	if (err != 0) {
		err = deluge_pthread_error(err);
		goto err_ctx;
	}

	this->root = root;
	this->id = id;
	this->programs = 0;

	return DELUGE_SUCCESS;
 err_ctx:
	clReleaseContext(this->ctx);
 err:
	return err;
}

void finlz_device(struct device *this)
{
	if ((this->programs & PROGRAM_BLAKE3) != 0)
		clReleaseProgram(this->blake3);
	pthread_mutex_destroy(&this->lock);
	clReleaseContext(this->ctx);
}

int init_device_blake3(struct device *this)
{
	int err;

	pthread_mutex_lock(&this->lock);

	if ((this->programs & PROGRAM_BLAKE3) == 0) {
		err = init_blake3_program(&this->blake3, this);
		if (err != DELUGE_SUCCESS)
			goto err;

		this->programs |= PROGRAM_BLAKE3;
	}

	err = DELUGE_SUCCESS;
 err:
	pthread_mutex_unlock(&this->lock);

	return err;
}

cl_program *get_device_blake3(struct device *this, int *err)
{
	int _err;

	if (err == NULL)
		err = &_err;

	*err = init_device_blake3(this);

	if (*err == DELUGE_SUCCESS)
		return &this->blake3;
	else
		return NULL;
}




#if 0
#include <deluge.h>
#include "deluge/deluge.h"
#include "deluge/device.h"
#include "deluge/error.h"
#include <stdio.h>


#define AVPROG_HIGHWAY   0x01
#define AVPROG_BLAKE3    0x02


static void __debug(const char *errinfo,
		    const void *privinfo __attribute__ ((unused)),
		    size_t cb __attribute__ ((unused)),
		    void *user __attribute__ ((unused)))
{
	fprintf(stderr, "DEVICE ERROR: %s\n", errinfo);
}

int init_device(struct device *this, struct deluge *root, cl_device_id devid)
{
	cl_ulong ulval;
	cl_uint uival;
	cl_int clret;
	int err;

	clret = clGetDeviceInfo(devid, CL_DEVICE_TYPE, sizeof (this->devtype),
				&this->devtype, NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	clret = clGetDeviceInfo(devid, CL_DEVICE_MAX_COMPUTE_UNITS,
				sizeof (uival), &uival, NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	} else {
		this->num_unit = uival;
	}

	clret = clGetDeviceInfo(devid, CL_DEVICE_GLOBAL_MEM_SIZE,
				sizeof (ulval), &ulval, NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	} else {
		this->total_gmem = ulval;
	}

	clret = clGetDeviceInfo(devid, CL_DEVICE_LOCAL_MEM_SIZE,
				sizeof (ulval), &ulval, NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	} else {
		this->max_lalloc = ulval;
		this->total_lmem = this->max_lalloc * this->num_unit;
	}

	clret = clGetDeviceInfo(devid, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE,
				sizeof (ulval), &ulval, NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	} else {
		this->total_cmem = ulval;
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
	this->used_cmem = 0;
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

size_t get_device_num_unit(const struct device *this)
{
	return this->num_unit;
}

size_t get_device_lmem_alloc(const struct device *this)
{
	return this->max_lalloc;
}

static size_t __alloc(struct device *this, const struct device_allocation *r)
{
	size_t cap, gcap, lcap;

	if (r->glen > 0)
		gcap = (this->total_gmem - this->used_gmem) / r->glen;
	else
		gcap = (size_t) -1;

	if (r->llen > 0)
		lcap = (this->total_lmem - this->used_lmem) / r->llen;
	else
		lcap = (size_t) -1;

	cap = gcap;

	if (lcap < cap)
		cap = lcap;

	if (r->clen > (this->total_cmem - this->used_cmem))
		cap = 0;

	if (r->mult > 0) {
		if (r->mult < cap)
			cap = r->mult;
		this->used_cmem += r->clen;
		this->used_gmem += cap * r->glen;
		this->used_lmem += cap * r->llen;
	}

	return cap;
}

size_t device_alloc(struct device *this, const struct device_allocation *r)
{
	size_t ret;

	if ((r->clen == 0) && (r->glen == 0) && (r->llen == 0))
		return 0;

	pthread_mutex_lock(&this->lock);
	ret = __alloc(this, r);
	pthread_mutex_unlock(&this->lock);

	return ret;
}

void device_free(struct device *this, const struct device_allocation *r)
{
	pthread_mutex_lock(&this->lock);

	this->used_cmem -= r->clen;
	this->used_gmem -= r->mult * r->glen;
	this->used_lmem -= r->mult * r->llen;

	pthread_mutex_unlock(&this->lock);
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

size_t get_device_cmem(struct device *this)
{
	size_t ret;

	pthread_mutex_lock(&this->lock);
	ret = this->total_cmem - this->used_cmem;
	pthread_mutex_unlock(&this->lock);

	return ret;
}

int alloc_on_device(struct device *this, size_t gmem, size_t lmem, size_t cmem)
{
	int ret;

	pthread_mutex_lock(&this->lock);

	if (this->total_gmem < (this->used_gmem + gmem)) {
		ret = DELUGE_NOMEM;
		goto out;
	}

	if (this->total_lmem < (this->used_lmem + lmem)) {
		ret = DELUGE_NOMEM;
		goto out;
	}

	if (this->total_cmem < (this->used_cmem + cmem)) {
		ret = DELUGE_NOMEM;
		goto out;
	}

	this->used_gmem += gmem;
	this->used_lmem += lmem;
	this->used_cmem += cmem;

	ret = DELUGE_SUCCESS;
 out:
	pthread_mutex_unlock(&this->lock);

	return ret;
}

void free_on_device(struct device *this, size_t gmem, size_t lmem, size_t cmem)
{
	pthread_mutex_lock(&this->lock);

	this->used_gmem -= gmem;
	this->used_lmem -= lmem;
	this->used_cmem -= cmem;

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


static int __get_device_blake3(struct device *this, cl_program *text)
{
	cl_int clret;
	int err;

	if ((this->avprogs & AVPROG_BLAKE3) == 0) {
		err = init_blake3_text(&this->blake3, this);
		if (err != DELUGE_SUCCESS)
			goto err;

		this->avprogs |= AVPROG_BLAKE3;
	}

	clret = clRetainProgram(this->blake3);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	*text = this->blake3;

	return DELUGE_SUCCESS;
 err:
	return err;
}

int get_device_blake3(struct device *this, cl_program *text)
{
	int err;

	pthread_mutex_lock(&this->lock);
	err = __get_device_blake3(this, text);
	pthread_mutex_unlock(&this->lock);

	return err;
}


/* static int __device_init_text(struct device *this, unsigned int textid) */
/* { */
/* 	int err; */

/* 	assert(textid < DEVICE_NUM_TEXT); */

/* 	if ((this->textavl & (1 << textid)) != 0) */
/* 		goto out; */

/* 	switch (textid) { */
/* 	case DEVICE_TEXT_BLAKE3: */
/* 		err = init_blake3_text(&this->texts[textid], this); */
/* 	} */

/* 	if (err != DELUGE_SUCCESS) */
/* 		goto err; */

/* 	this->textavl |= (1 << textid); */

/*  out: */
/* 	return DELUGE_SUCCESS; */
/*  err: */
/* 	return err; */
/* } */

/* int device_init_text(struct device *this, unsigned int textid) */
/* { */
/* 	int err; */

/* 	pthread_mutex_lock(&this->lock); */
/* 	err = __device_init_text(this, textid); */
/* 	pthread_mutex_unlock(&this->lock); */

/* 	return err; */
/* } */

/* static int __device_init_cost(struct device *this, unsigned int costid) */
/* { */
/* 	unsigned int textid; */
/* 	int err; */

/* 	assert(costid < DEVICE_NUM_COST); */

/* 	if ((this->costavl & (1 << costid)) != 0) */
/* 		goto out; */

/* 	switch (costid) { */
/* 	case DEVICE_COST_HASHSUM64_BLAKE3: */
/* 		textid = DEVICE_TEXT_BLAKE3; */
/* 	} */

/* 	err = __device_init_text(this, textid); */
/* 	if (err != DELUGE_SUCCESS) */
/* 		goto err; */

/* 	switch (costid) { */
/* 	case DEVICE_COST_HASHSUM64_BLAKE3: */
/* 		ret = init_hashsum_blake3_cost(&this->costs[textid], this); */
/* 	} */

/* 	if (ret == DELUGE_SUCCESS) */
/* 		this->textavl |= (1 << textid); */

/*  out: */
/* 	pthread_mutex_unlock(&this->lock); */
/* 	return ret; */

/* } */

/* int device_init_cost(struct device *this, unsigned int costid) */
/* { */
/* 	int err; */

/* 	pthread_mutex_lock(&this->lock); */
/* 	err = __device_init_cost(this, costid); */
/* 	pthread_mutex_unlock(&this->lock); */

/* 	return err; */
/* } */
#endif
