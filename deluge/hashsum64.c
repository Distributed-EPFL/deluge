#include <assert.h>
#include <deluge.h>
#include "deluge/dispatch.h"
#include "deluge/error.h"
#include "deluge/hashsum64.h"
#include "deluge/uint.h"
#include "deluge/util.h"
#include <string.h>


#define INITIAL_JOBCAP  ((1ul << 12) / sizeof (struct hashsum64_job))


struct hashsum64_job
{
	const uint64_t *restrict elems;
	size_t                   nelem;
	void                   (*cb)(int, void *, void *);
	void                    *user;
};


#define BACKEND_IDLE  0    /* does nothing and can be acquired */
#define BACKEND_BUSY  1    /* begin setup by host, cannot be acquired */
#define BACKEND_EXEC  2    /* worker is running, cannot be acquired */

struct hashsum64_backend
{
	struct hashsum64      *dispatch;  /* parent dispatch */
	struct device         *dev;       /* associated device */
	size_t                 capacity;  /* maximum number of elements */
	pthread_mutex_t        lock;      /* lock for worker sync */
	int                    state;     /* IDLE, BUSY or EXEC */
	int                    active;    /* `0` when worker has to stop */

	/* following fields are valid only if `capacity > 0` */
	cl_kernel              entry;     /* entry kernel */
	cl_command_queue       queue;     /* command queue */
	size_t                 wgsize;    /* workgroup size */
	size_t                 cstlen;    /* size of constants */
	cl_mem                 devcst;    /* device memory for constants */
	cl_mem                 devin;     /* device memory for input */
	cl_mem                 hostin;    /* host (pinned) memory for input */
	uint64_t              *ptrin;     /* mapped `hostin` */
	cl_mem                 devout;    /* device memory for output */
	cl_mem                 hostout;   /* host (pinned) memory for output */
	uint320_t             *ptrout;    /* mapped `hostout` */
	pthread_t              worker;    /* worker thread */
	pthread_cond_t         cond;      /* worker wakeup condition */

	/* following fields are valid only if `state == BACKEND_BUSY` */
	struct hashsum64_job   current;   /* current job */
};

static void exec_backend_job(struct hashsum64_backend *this)
{
	uint8_t res[DELUGE_HASHSUM64_LEN];
	size_t bsize, lsize, gsize, ngrp;
	uint64_t nelem;
	cl_int clret;
	int err;

	nelem = this->current.nelem;
	bsize = this->current.nelem * sizeof (*this->current.elems);
	
	lsize = this->wgsize;

	ngrp = this->current.nelem / lsize;
	if ((ngrp * lsize) < this->current.nelem)
		ngrp += 1;

	gsize = ngrp * lsize;

	clret = clSetKernelArg(this->entry, 0, sizeof (uint64_t), &nelem);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	memcpy(this->ptrin, this->current.elems, bsize);

	clret = clEnqueueWriteBuffer(this->queue, this->devin, CL_TRUE,
				     0, bsize, this->ptrin, 0, NULL, NULL);
 	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	clret = clEnqueueNDRangeKernel(this->queue, this->entry,
				       1, NULL, &gsize, &lsize, 0, NULL, NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	clret = clEnqueueReadBuffer(this->queue, this->devout, CL_TRUE,
				    0, ngrp * sizeof (*this->ptrout),
				    this->ptrout, 0, NULL, NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	uint320_sum(this->ptrout, ngrp);
	uint320_dump_be(res, &this->ptrout[0]);

	this->current.cb(DELUGE_SUCCESS, res, this->current.user);

	return;
 err:
	this->current.cb(err, NULL, this->current.user);
}

static int resize_backend(struct hashsum64_backend *this, size_t capacity);

static int __dequeue_job(struct hashsum64 *this, struct hashsum64_job *dest);

static void exec_backend(struct hashsum64_backend *this)
{
	struct hashsum64_job *current = &this->current;
	int err, morejob;

	do {
		if (this->capacity < this->current.nelem) {
			err = resize_backend(this, current->nelem);
			if (err != DELUGE_SUCCESS) {
				current->cb(err, NULL, current->user);
				goto next;
			}
		}

		exec_backend_job(this);

	next:
		pthread_mutex_lock(&this->dispatch->joblock);

		morejob = __dequeue_job(this->dispatch, current);
	
		pthread_mutex_unlock(&this->dispatch->joblock);
	} while (morejob);
}

static void *run_backend(void *_this)
{
	struct hashsum64_backend *this = _this;

	pthread_mutex_lock(&this->lock);

	while (this->active) {
		if (this->state != BACKEND_EXEC) {
			pthread_cond_wait(&this->cond, &this->lock);
			continue;
		}

		pthread_mutex_unlock(&this->lock);

		exec_backend(this);

		pthread_mutex_lock(&this->lock);

		this->state = BACKEND_IDLE;
	}

	this->state = BACKEND_BUSY;

	pthread_mutex_unlock(&this->lock);

	return NULL;
}

static void submit_backend_job(struct hashsum64_backend *this,
			       const uint64_t *restrict elems, size_t nelem,
			       void (*cb)(int, void *, void *), void *user)
{
	assert(this->state == BACKEND_BUSY);

	this->current.elems = elems;
	this->current.nelem = nelem;
	this->current.cb = cb;
	this->current.user = user;

	pthread_mutex_lock(&this->lock);

	this->state = BACKEND_EXEC;
	pthread_cond_signal(&this->cond);

	pthread_mutex_unlock(&this->lock);
}

static int populate_backend(struct hashsum64_backend *this, size_t capacity)
{
	cl_int clret;
	size_t ngrp;
	int err;

	ngrp = capacity / this->wgsize;
	if ((ngrp * this->wgsize) < capacity)
		ngrp += 1;
	capacity = ngrp * this->wgsize;

	this->devin = clCreateBuffer(this->dev->ctx,
				     CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY,
				     capacity * sizeof (uint64_t), NULL,
				     &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	this->hostin = clCreateBuffer(this->dev->ctx,
				      CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR,
				      capacity * sizeof (uint64_t), NULL,
				      &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_devin;
	}

	this->ptrin = clEnqueueMapBuffer(this->queue, this->hostin, CL_TRUE,
					 CL_MAP_WRITE,
					 0, capacity * sizeof (uint64_t),
					 0, NULL, NULL, &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_hostin;
	}

	this->devout = clCreateBuffer(this->dev->ctx,
				      CL_MEM_WRITE_ONLY |CL_MEM_HOST_READ_ONLY,
				      ngrp * sizeof (uint320_t), NULL,
				      &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_hostin;
	}

	this->hostout = clCreateBuffer(this->dev->ctx, CL_MEM_ALLOC_HOST_PTR,
				       ngrp * sizeof (uint320_t), NULL,
				       &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_devout;
	}

	this->ptrout = clEnqueueMapBuffer(this->queue, this->hostout, CL_TRUE,
					  CL_MAP_READ,
					  0, ngrp * sizeof (uint320_t),
					  0, NULL, NULL, &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_hostout;
	}

	clret = clSetKernelArg(this->entry, 2, sizeof (this->devin),
			       &this->devin);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_hostout;
	}

	clret = clSetKernelArg(this->entry, 3, sizeof (this->devout),
			       &this->devout);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_hostout;
	}

	this->capacity = capacity;

	return DELUGE_SUCCESS;
 err_hostout:
	clReleaseMemObject(this->hostout);
 err_devout:
	clReleaseMemObject(this->devout);
 err_hostin:
	clReleaseMemObject(this->hostin);
 err_devin:
	clReleaseMemObject(this->devin);
 err:
	return err;
}

static void depopulate_backend(struct hashsum64_backend *this)
{
	clReleaseMemObject(this->hostout);
	clReleaseMemObject(this->devout);
	clReleaseMemObject(this->hostin);
	clReleaseMemObject(this->devin);
}

static int start_backend(struct hashsum64_backend *this)
{
	void *scratch;
	cl_int clret;
	int err;

	assert(this->state == BACKEND_BUSY);

	err = this->dispatch->vtable.setup(this->dispatch, NULL, &this->cstlen,
					   this->dev);
	if (err != DELUGE_SUCCESS)
		goto err;

	if (this->cstlen > 0) {
		scratch = malloc(this->cstlen);
		if (scratch == NULL) {
			err = deluge_c_error();
			goto err;
		}

		err = this->dispatch->vtable.setup(this->dispatch, scratch,
						   &this->cstlen, this->dev);
		if (err != DELUGE_SUCCESS)
			goto err;
	} else {
		scratch = NULL;
	}

	err = this->dispatch->vtable.prepare(this->dispatch, &this->entry,
					     this->dev);
	if (err != DELUGE_SUCCESS)
		goto err_scratch;

	clret = clGetKernelWorkGroupInfo(this->entry, this->dev->id,
					 CL_KERNEL_WORK_GROUP_SIZE,
					 sizeof (this->wgsize), &this->wgsize,
					 NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_entry;
	}

	clret = clSetKernelArg(this->entry, 1,
			       this->wgsize * sizeof (uint320_t), NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_entry;
	}

	this->queue = clCreateCommandQueueWithProperties(this->dev->ctx,
							 this->dev->id, NULL,
							 &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_entry;
	}

	if (this->cstlen > 0) {
		this->devcst = clCreateBuffer(this->dev->ctx,
					      CL_MEM_COPY_HOST_PTR |
					      CL_MEM_HOST_NO_ACCESS,
					      this->cstlen, scratch, &clret);
		if (clret != CL_SUCCESS) {
			err = deluge_cl_error(clret);
			goto err_queue;
		}

		clret = clSetKernelArg(this->entry, 4, sizeof (this->devcst),
				       &this->devcst);
		if (clret != CL_SUCCESS) {
			err = deluge_cl_error(clret);
			goto err_devcst;
		}
	}

	err = pthread_cond_init(&this->cond, NULL);
	if (err != 0) {
		err = deluge_pthread_error(err);
		goto err_devcst;
	}

	this->active = 1;

	err = pthread_create(&this->worker, NULL, run_backend, this);
	if (err != 0) {
		err = deluge_pthread_error(err);
		goto err_cond;
	}

	free(scratch);

	return DELUGE_SUCCESS;
 err_cond:
	this->active = 0;
	pthread_cond_destroy(&this->cond);
 err_devcst:
	if (this->cstlen > 0)
		clReleaseMemObject(this->devcst);
 err_queue:
	clReleaseCommandQueue(this->queue);
 err_entry:
	clReleaseKernel(this->entry);
 err_scratch:
	free(scratch);
 err:
	return err;
}

static void stop_backend(struct hashsum64_backend *this)
{
	pthread_mutex_lock(&this->lock);

	this->active = 0;
	pthread_cond_signal(&this->cond);

	pthread_mutex_unlock(&this->lock);

	pthread_join(this->worker, NULL);

	pthread_cond_destroy(&this->cond);
	if (this->cstlen > 0)
		clReleaseMemObject(this->devcst);
	clReleaseCommandQueue(this->queue);
	clReleaseKernel(this->entry);

	this->capacity = 0;
}

static int resize_backend(struct hashsum64_backend *this, size_t capacity)
{
	int err;

	assert(this->state == BACKEND_BUSY);

	if (this->capacity == 0) {
		err = start_backend(this);
		if (err != DELUGE_SUCCESS)
			goto err;
	} else {
		depopulate_backend(this);
	}

	err = populate_backend(this, capacity);
	if (err != DELUGE_SUCCESS)
		goto err_stop;

	return DELUGE_SUCCESS;
 err_stop:
	stop_backend(this);
 err:
	return err;
}

static int init_backend(struct hashsum64_backend *this,
			struct hashsum64 *dispatch, struct device *dev)
{
	int err;

	err = pthread_mutex_init(&this->lock, NULL);
	if (err != 0) {
		err = deluge_pthread_error(err);
		goto err;
	}

	this->dispatch = dispatch;
	this->dev = dev;
	this->capacity = 0;
	this->state = BACKEND_IDLE;
	this->active = 0;

	return DELUGE_SUCCESS;
 err:
	return err;
}

static void finlz_backend(struct hashsum64_backend *this)
{
	if (this->capacity > 0) {
		depopulate_backend(this);
		stop_backend(this);
	}

	pthread_mutex_destroy(&this->lock);
}


static void __destroy(struct deluge_dispatch *_this)
{
	struct hashsum64 *this = container_of(_this, struct hashsum64, super);

	this->vtable.destroy(this);
}


int init_hashsum64(struct hashsum64 *this,
		   const struct hashsum64_vtable *vtable)
{
	static struct dispatch_vtable _vtable = {
		.destroy = __destroy
	};
	struct deluge *root;
	size_t i;
	int err;

	err = init_dispatch(&this->super, &_vtable);
	if (err != DELUGE_SUCCESS)
		goto err;

	root = this->super.root;

	this->backends = malloc(root->ndevice * sizeof (*this->backends));
	if (this->backends == NULL)
		goto err_super;

	for (i = 0; i < root->ndevice; i++) {
		err = init_backend(&this->backends[i], this,
				   &root->devices[i]);
		if (err != DELUGE_SUCCESS)
			goto err_backends;
	}

	err = pthread_mutex_init(&this->joblock, NULL);
	if (err != 0) {
		err = deluge_pthread_error(err);
		goto err_backends;
	}

	this->jobs = NULL;
	this->jobcap = 0;
	this->jobhead = 0;
	this->jobtail = 0;

	this->vtable = *vtable;

	return DELUGE_SUCCESS;
 err_backends:
	while (i-- > 0)
		finlz_backend(&this->backends[i]);
	free(this->backends);
 err_super:
	finlz_dispatch(&this->super);
 err:
	return err;
}

void finlz_hashsum64(struct hashsum64 *this)
{
	struct deluge *root = this->super.root;
	struct hashsum64_job job;
	size_t i;

	pthread_mutex_lock(&this->joblock);

	while (__dequeue_job(this, &job))
		job.cb(DELUGE_CANCEL, NULL, job.user);

	pthread_mutex_unlock(&this->joblock);

	for (i = 0; i < root->ndevice; i++)
		finlz_backend(&this->backends[i]);

	free(this->backends);

	pthread_mutex_destroy(&this->joblock);
	free(this->jobs);

	finlz_dispatch(&this->super);
}

static int __grow_jobs(struct hashsum64 *this)
{
	size_t newjobcap, head, tail, len;
	struct hashsum64_job *newjobs;
	int err;

	if (this->jobcap == 0)
		newjobcap = INITIAL_JOBCAP;
	else
		newjobcap = this->jobcap * 2;

	newjobs = malloc(newjobcap * sizeof (*newjobs));
	if (newjobs == NULL) {
		err = deluge_c_error();
		goto err;
	}

	head = this->jobhead;
	tail = this->jobtail;

	if (tail <= head) {
		len = head - tail;
		memcpy(newjobs, &this->jobs[tail], len * sizeof (*newjobs));
	} else {
		len = this->jobcap - tail;
		memcpy(newjobs, &this->jobs[tail], len * sizeof (*newjobs));
		memcpy(&newjobs[len], this->jobs, head * sizeof (*newjobs));
		len += head;
	}

	free(this->jobs);

	this->jobs = newjobs;
	this->jobtail = 0;
	this->jobhead = len;
	this->jobcap = newjobcap;

	return DELUGE_SUCCESS;
 err:
	return err;
}

static int __enqueue_job(struct hashsum64 *this,
			 const uint64_t *restrict elems, size_t nelem,
			 void (*cb)(int, void *, void *), void *user)
{
	size_t head = this->jobhead;
	size_t tail = this->jobtail;
	int err;

	if (head >= tail)
		tail += this->jobcap;

	if ((tail - head) <= 1) {
		err = __grow_jobs(this);
		if (err != DELUGE_SUCCESS)
			goto err;
		head = this->jobhead;
	}

	this->jobs[head].elems = elems;
	this->jobs[head].nelem = nelem;
	this->jobs[head].cb = cb;
	this->jobs[head].user = user;

	this->jobhead = (head + 1) % this->jobcap;

	return DELUGE_SUCCESS;
 err:
	return err;
}

static int __dequeue_job(struct hashsum64 *this, struct hashsum64_job *dest)
{
	size_t tail = this->jobtail;

	if (tail == this->jobhead)
		return 0;

	*dest = this->jobs[tail];
	this->jobtail = (tail + 1) % this->jobcap;

	return 1;
}

static struct hashsum64_backend *acquire_idle_backend(struct hashsum64 *this)
{
	struct deluge *root = this->super.root;
	struct hashsum64_backend *backend;
	int found;
	size_t i;

	found = 0;

	for (i = 0; i < root->ndevice; i++) {
		backend = &this->backends[i];

		pthread_mutex_lock(&backend->lock);

		if (backend->state == BACKEND_IDLE) {
			backend->state = BACKEND_BUSY;
			found = 1;
		}

		pthread_mutex_unlock(&backend->lock);

		if (found)
			return backend;
	}

	return NULL;
}						  

static void release_backend(struct hashsum64_backend *this)
{
	pthread_mutex_lock(&this->lock);
	this->state = BACKEND_BUSY;
	pthread_mutex_unlock(&this->lock);
}

int deluge_hashsum64_schedule(deluge_dispatch_t _this,
			      const uint64_t *restrict elems, size_t nelem,
			      void (*cb)(int, void *, void *), void *user)
{
	struct hashsum64_backend *backend;
	struct hashsum64 *this;
	int err;

	this = container_of(_this, struct hashsum64, super);

	pthread_mutex_lock(&this->joblock);

	backend = acquire_idle_backend(this);

	if (backend == NULL) {
		err = __enqueue_job(this, elems, nelem, cb, user);

		pthread_mutex_unlock(&this->joblock);

		if (err != DELUGE_SUCCESS)
			goto err;

		return DELUGE_SUCCESS;
	} else {
		pthread_mutex_unlock(&this->joblock);
	}

	if (backend->capacity < nelem) {
		err = resize_backend(backend, nelem);
		if (err != DELUGE_SUCCESS)
			goto err_backend;
	}

	submit_backend_job(backend, elems, nelem, cb, user);

	return DELUGE_SUCCESS;
 err_backend:
	release_backend(backend);
 err:
	return err;
}
