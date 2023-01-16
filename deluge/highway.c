#include <deluge.h>
#include "deluge/deluge.h"
#include "deluge/device.h"
#include "deluge/error.h"
#include "deluge/highway.h"
#include "deluge/list.h"
#include "deluge/opencl.h"
#include "deluge/uint.h"
#include <pthread.h>
#include <string.h>


#define ARRAY_SIZE(_arr)  (sizeof (_arr) / sizeof (*(_arr)))

#define COMPILE_OPTIONS   "-Werror -cl-std=CL3.0"

#define HASHSUM_KNAME     "hash_sum"
#define HASHSUM_MAXLEN    (1ul << 18)


struct __source
{
	const char *name;
	const char *start;
	const char *end;
};

struct station
{
	struct highway_program  *prog;
	cl_command_queue         queue;
	cl_kernel                hashsum;
	cl_mem                   initial;
	cl_mem                   input;
	cl_mem                   output;
	uint320_t               *partsums;
	struct list              stqueue;
};

struct job
{
	const uint64_t *input;
	size_t ninput;
	size_t npart;
	void *user;
	void (*cb)(int, uint64_t[5], void *);
	struct list queue;
	struct deluge_highway *dispatch;
	struct station *station;
	cl_event wrev;
	cl_event exev;
	cl_event rdev;
};

struct deluge_highway
{
	struct deluge    *root;
	uint64_t          key[4];
	pthread_mutex_t   qlock;
	int               stopping;
	struct list       stidle;
	struct list       stbusy;
	struct list       jobqueue;
};


extern const char _binary_deluge_highway_cl_start[];
extern const char _binary_deluge_highway_cl_end[];

extern const char _binary_deluge_highway_h_start[];
extern const char _binary_deluge_highway_h_end[];

extern const char _binary_deluge_opencl_h_start[];
extern const char _binary_deluge_opencl_h_end[];

extern const char _binary_deluge_uint_cl_start[];
extern const char _binary_deluge_uint_cl_end[];

extern const char _binary_deluge_uint_h_start[];
extern const char _binary_deluge_uint_h_end[];


static const struct __source __headers[] = {
	{
		"deluge/highway.h",
		_binary_deluge_highway_h_start,
		_binary_deluge_highway_h_end
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

static const struct __source __sources[] = {
	{
		"deluge/highway.cl",
		_binary_deluge_highway_cl_start,
		_binary_deluge_highway_cl_end
	},
	{
		"deluge/uint.cl",
		_binary_deluge_uint_cl_start,
		_binary_deluge_uint_cl_end
	}
};


static void release_station(struct deluge_highway *this, struct station *s);


static int init_source(struct device *dev, const char **ns, cl_program *ps,
		       const struct __source *src)
{
	const char *text;
	cl_int clret;
	size_t size;

	if (ns != NULL)
		*ns = src->name;

	text = src->start;
	size = src->end - text;
	*ps = clCreateProgramWithSource(dev->ctx, 1, &text, &size, &clret);

	if (clret != CL_SUCCESS)
		return deluge_cl_error(clret);

	return DELUGE_SUCCESS;
}

static int init_program_cost(struct highway_program *this)
{
	cl_kernel hashsum;
	cl_int clret;
	int err;

	hashsum = clCreateKernel(this->prog, HASHSUM_KNAME, &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	this->hashsum_wg_size = 0;

	clret = clGetKernelWorkGroupInfo(hashsum, this->dev->devid,
					 CL_KERNEL_WORK_GROUP_SIZE,
					 sizeof (this->hashsum_wg_size),
					 &this->hashsum_wg_size, NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_hashsum;
	}

	this->hashsum_wg_max = HASHSUM_MAXLEN / this->hashsum_wg_size;
	if ((this->hashsum_wg_max * this->hashsum_wg_size) < HASHSUM_MAXLEN)
		this->hashsum_wg_max += 1;

	this->hashsum_gmem_input_size = HASHSUM_MAXLEN * sizeof (uint64_t);
	this->hashsum_gmem_output_size =
		this->hashsum_wg_max * sizeof (uint320_t);
	this->hashsum_lmem_size = this->hashsum_wg_size * sizeof (uint320_t);

	clReleaseKernel(hashsum);

	return DELUGE_SUCCESS;
 err_hashsum:
	clReleaseKernel(hashsum);
 err:
	return err;
}

int init_highway_program(struct highway_program *this, struct device *dev)
{
	const char *header_names[ARRAY_SIZE(__headers)];
	const char *source_names[ARRAY_SIZE(__sources)];
	cl_program headers[ARRAY_SIZE(__headers)];
	cl_program sources[ARRAY_SIZE(__sources)];
	cl_int clret;
	size_t i;
	int err;

	for (i = 0; i < ARRAY_SIZE(__headers); i++) {
		err = init_source(dev, &header_names[i], &headers[i],
				  &__headers[i]);
		if (err != DELUGE_SUCCESS)
			goto err_headers;
	}

	for (i = 0; i < ARRAY_SIZE(__sources); i++) {
		err = init_source(dev, &source_names[i], &sources[i],
				  &__sources[i]);
		if (err != DELUGE_SUCCESS)
			goto err_sources;
	}

	for (i = 0; i < ARRAY_SIZE(sources); i++) {
		clret = clCompileProgram(sources[i], 1, &dev->devid,
					 COMPILE_OPTIONS, ARRAY_SIZE(headers),
					 headers, header_names, NULL, NULL);
		if (clret != CL_SUCCESS) {
			err = deluge_cl_compile_error(clret, source_names[i],
						      sources[i], dev->devid);
			goto err_all_sources;
		}
	}

	this->prog = clLinkProgram(dev->ctx, 1, &dev->devid, NULL,
				   ARRAY_SIZE(sources), sources, NULL, NULL,
				   &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_link_error(clret, this->prog, source_names,
					   sources, ARRAY_SIZE(sources),
					   dev->devid);
		goto err_all_sources;
	}

	this->dev = dev;

	err = init_program_cost(this);
	if (err != DELUGE_SUCCESS)
		goto err_prog;

	for (i = 0; i < ARRAY_SIZE(sources); i++)
		clReleaseProgram(sources[i]);
	for (i = 0; i < ARRAY_SIZE(headers); i++)
		clReleaseProgram(headers[i]);

	return DELUGE_SUCCESS;
 err_prog:
	clReleaseProgram(this->prog);
 err_all_sources:
	i = ARRAY_SIZE(__sources);
 err_sources:
	while (i-- > 0)
		clReleaseProgram(sources[i]);
	i = ARRAY_SIZE(__headers);
 err_headers:
	while (i-- > 0)
		clReleaseProgram(headers[i]);
	return err;
}

void finlz_highway_program(struct highway_program *this)
{
	clReleaseProgram(this->prog);
}

static size_t get_program_capacity(const struct highway_program *this)
{
	size_t dev_gmem, dev_lmem;
	size_t gcap, lcap;

	dev_gmem = get_device_gmem(this->dev);
	gcap = dev_gmem / (this->hashsum_gmem_input_size +
			   this->hashsum_gmem_output_size);

	dev_lmem = get_device_lmem(this->dev);
	lcap = dev_lmem / this->hashsum_lmem_size;

	if (gcap < lcap)
		return gcap;

	return lcap;
}

static int alloc_program(const struct highway_program *this)
{
	size_t gmem, lmem;

	gmem = this->hashsum_gmem_input_size + this->hashsum_gmem_output_size;
	lmem = this->hashsum_lmem_size;

	return alloc_on_device(this->dev, gmem, lmem);
}

static void free_program(const struct highway_program *this)
{
	size_t gmem, lmem;

	gmem = this->hashsum_gmem_input_size + this->hashsum_gmem_output_size;
	lmem = this->hashsum_lmem_size;

	return free_on_device(this->dev, gmem, lmem);
}


static void reset_state(highway_t *st, const uint256_t *restrict key)
{
        uint32_t half0, half1;
        int i;

	st->mul0[0] = 0xdbe6d5d5fe4cce2full;
        st->mul0[1] = 0xa4093822299f31d0ull;
        st->mul0[2] = 0x13198a2e03707344ull;
        st->mul0[3] = 0x243f6a8885a308d3ull;
        st->mul1[0] = 0x3bd39e10cb0ef593ull;
        st->mul1[1] = 0xc0acf169b5f18a8cull;
        st->mul1[2] = 0xbe5466cf34e90c6cull;
        st->mul1[3] = 0x452821e638d01377ull;

        st->v0[0] = (st->mul0[0] ^ key->arr[0]) + 0x800000008;
        st->v0[1] = (st->mul0[1] ^ key->arr[1]) + 0x800000008;
	st->v0[2] = (st->mul0[2] ^ key->arr[2]) + 0x800000008;
	st->v0[3] = (st->mul0[3] ^ key->arr[3]) + 0x800000008;

	st->v1[0] = st->mul1[0] ^ ((key->arr[0] >> 32) | (key->arr[0] << 32));
        st->v1[1] = st->mul1[1] ^ ((key->arr[1] >> 32) | (key->arr[1] << 32));
        st->v1[2] = st->mul1[2] ^ ((key->arr[2] >> 32) | (key->arr[2] << 32));
        st->v1[3] = st->mul1[3] ^ ((key->arr[3] >> 32) | (key->arr[3] << 32));

        for (i = 0; i < 4; ++i) {
                half0 = st->v1[i] & 0xffffffff;
                half1 = (st->v1[i] >> 32);
                st->v1[i] = (half0 << 8) | (half0 >> 24);
                st->v1[i] |= (uint64_t) ((half1 << 8) | (half1 >> 24)) << 32;
        }
}

static int init_station(struct station *this, struct highway_program *prog,
			const uint64_t key[4])
{
	struct device *dev = prog->dev;
	highway_t initial;
	uint256_t key256;
	cl_int clret;
	int err;

	uint256_init_le64(&key256, key);
	reset_state(&initial, &key256);

	this->hashsum = clCreateKernel(prog->prog, HASHSUM_KNAME, &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	this->initial = clCreateBuffer(dev->ctx,
				       CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				       sizeof (initial), &initial, &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_hashsum;
	}

	this->input = clCreateBuffer(dev->ctx, CL_MEM_READ_ONLY,
				     prog->hashsum_gmem_input_size, NULL,
				     &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_initial;
	}

	this->output = clCreateBuffer(dev->ctx, CL_MEM_WRITE_ONLY,
				      prog->hashsum_gmem_output_size, NULL,
				      &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_input;
	}

	this->queue = clCreateCommandQueueWithProperties(dev->ctx, dev->devid,
							 NULL, &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_output;
	}

	this->partsums = malloc(prog->hashsum_gmem_output_size);
	if (this->partsums == NULL) {
		err = deluge_c_error();
		goto err_queue;
	}

	clret = clSetKernelArg(this->hashsum, 1, sizeof (this->input),
			       &this->input);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_partsums;
	}

	clret = clSetKernelArg(this->hashsum, 2, sizeof (this->initial),
			       &this->initial);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_partsums;
	}

	clret = clSetKernelArg(this->hashsum, 3, sizeof (this->output),
			       &this->output);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_partsums;
	}

	clret = clSetKernelArg(this->hashsum, 4, prog->hashsum_lmem_size,
			       NULL);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_partsums;
	}

	this->prog = prog;
	list_init(&this->stqueue);

	return DELUGE_SUCCESS;
 err_partsums:
	free(this->partsums);
 err_queue:
	clReleaseCommandQueue(this->queue);
 err_output:
	clReleaseMemObject(this->output);
 err_input:
	clReleaseMemObject(this->input);
 err_initial:
	clReleaseMemObject(this->initial);
 err_hashsum:
	clReleaseKernel(this->hashsum);
 err:
	return err;
}

static void finlz_station(struct station *this)
{
	clFinish(this->queue);
	free(this->partsums);
	clReleaseCommandQueue(this->queue);
	clReleaseMemObject(this->output);
	clReleaseMemObject(this->input);
	clReleaseMemObject(this->initial);
	clReleaseKernel(this->hashsum);
}

static int alloc_station(struct device *dev, uint64_t key[4], struct list *dst)
{
	struct station *station;
	int err;

	station = malloc(sizeof (*station));
	if (station == NULL) {
		err = deluge_c_error();
		goto err;
	}

	err = init_station(station, &dev->highway, key);
	if (err != DELUGE_SUCCESS)
		goto err_station;

	list_push(dst, &station->stqueue);

	return DELUGE_SUCCESS;
 err_station:
	free(station);
 err:
	return err;
}

static void free_station(struct station *station)
{
	finlz_station(station);
	free(station);
}

static void cancel_job(struct job *job)
{
	uint64_t dummy[5];

	job->cb(DELUGE_CANCEL, dummy, job->user);

	free(job);
}

static void complete_job(cl_event ev __attribute__ ((unused)),
			 cl_int status __attribute__ ((unused)), void *ujob)
{
	struct job *job = ujob;
	struct station *st = job->station;
	uint64_t result[5];

	uint320_sum(st->partsums, job->npart);

	memcpy(result, st->partsums[0].arr, sizeof (result));

	clReleaseEvent(job->rdev);
	clReleaseEvent(job->exev);
	clReleaseEvent(job->wrev);

	job->cb(DELUGE_SUCCESS, result, job->user);

	release_station(job->dispatch, st);

	free(job);
}

static int launch_job(struct station *this, struct job *job)
{
	size_t gsize, lsize, ngrp;
	cl_int clret;
	int err;

	lsize = this->prog->hashsum_wg_size;
	gsize = job->ninput;
	ngrp = gsize / lsize;
	if ((gsize % lsize) != 0) {
		ngrp += 1;
		gsize = ngrp * lsize;
	}

	clret = clEnqueueWriteBuffer(this->queue, this->input, CL_FALSE, 0,
				     job->ninput * sizeof (*job->input),
				     job->input, 0, NULL, &job->wrev);
 	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err;
	}

	clret = clSetKernelArg(this->hashsum, 0, sizeof (job->ninput),
			       &job->ninput);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_wrev;
	}

	clret = clEnqueueNDRangeKernel(this->queue, this->hashsum,
				     1, NULL, &gsize, &lsize,
				     1, &job->wrev, &job->exev);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_wrev;
	}

	clret = clEnqueueReadBuffer(this->queue, this->output, CL_FALSE,
				    0, ngrp * sizeof (uint320_t),
				    this->partsums, 1, &job->exev, &job->rdev);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_exev;
	}

	job->station = this;
	job->npart = ngrp;

	clret = clSetEventCallback(job->rdev, CL_COMPLETE, complete_job, job);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_error(clret);
		goto err_rdev;
	}

	return DELUGE_SUCCESS;
 err_rdev:
	clReleaseEvent(job->rdev);
 err_exev:
	clReleaseEvent(job->exev);
 err_wrev:
	clReleaseEvent(job->wrev);
 err:
	return err;
}

static int init_dispatch(struct deluge_highway *this, struct deluge *root,
			 const uint64_t key[4])
{
	size_t i;
	int err;

	for (i = 0; i < root->ndevice; i++) {
		if (has_device_highway(&root->devices[i]))
			continue;

		err = init_device_highway(&root->devices[i]);
		if (err != DELUGE_SUCCESS)
			goto err;
	}

	memcpy(this->key, key, sizeof (this->key));

	err = pthread_mutex_init(&this->qlock, NULL);
	if (err != 0) {
		err = deluge_c_error();
		goto err;
	}

	this->stopping = 0;
	list_init(&this->stidle);
	list_init(&this->stbusy);
	list_init(&this->jobqueue);

	this->root = retain_deluge(root);

	return DELUGE_SUCCESS;
 err:
	return err;
}

static void finlz_dispatch(struct deluge_highway *this)
{
	struct station *st;
	struct list *elem;

	while ((elem = list_pop(&this->stidle)) != NULL) {
		st = list_item(elem, struct station, stqueue);
		free_program(st->prog);
		free_station(st);
	}

	release_deluge(this->root);
	pthread_mutex_destroy(&this->qlock);
}

int deluge_highway_create(deluge_t deluge, deluge_highway_t *highway,
			  const uint64_t key[4])
{
	struct deluge_highway *this;
	int err;

	this = malloc(sizeof (*this));
	if (this == NULL) {
		err = deluge_c_error();
		goto err;
	}

	err = init_dispatch(this, deluge, key);
	if (err != DELUGE_SUCCESS)
		goto err_this;

	*highway = this;

	return DELUGE_SUCCESS;
 err_this:
	free(this);
 err:
	*highway = NULL;  /* make gcc happy */
	return err;
}

void deluge_highway_destroy(deluge_highway_t highway)
{
	struct list *elem;
	int idle;

	pthread_mutex_lock(&highway->qlock);

	highway->stopping = 1;

	while ((elem = list_pop(&highway->jobqueue)) != NULL)
		cancel_job(list_item(elem, struct job, queue));

	idle = list_empty(&highway->stbusy);

	pthread_mutex_unlock(&highway->qlock);

	if (idle) {
		finlz_dispatch(highway);
		free(highway);
	}
}

size_t deluge_highway_space(deluge_highway_t highway)
{
	struct deluge *root = highway->root;
	size_t i, cap;

	cap = 0;
	for (i = 0; i < root->ndevice; i++)
		cap += get_program_capacity(&root->devices[i].highway);

	return cap;
}

int deluge_highway_alloc(deluge_highway_t highway, size_t len)
{
	struct deluge *root = highway->root;
	struct list nlist, *elem;
	struct device **devs;
	size_t i, devidx;
	int err;

	devs = malloc(len * sizeof (*devs));
	if (devs == NULL) {
		err = deluge_c_error();
		goto err;
	}

	devidx = 0;
	for (i = 0; i < len; i++) {
		err = DELUGE_NODEV;

		while (devidx < root->ndevice) {
			err = alloc_program(&root->devices[devidx].highway);
			if (err == DELUGE_SUCCESS) {
				devs[i] = &root->devices[devidx];
				break;
			} else {
				devidx += 1;
			}
		}

		if (devidx == root->ndevice)
			goto err_program;
	}

	list_init(&nlist);

	for (i = 0; i < len; i++) {
		err = alloc_station(devs[i], highway->key, &nlist);
		if (err != DELUGE_SUCCESS)
			goto err_station;
	}

	pthread_mutex_lock(&highway->qlock);
	list_append(&highway->stidle, &nlist);
	pthread_mutex_unlock(&highway->qlock);

	free(devs);

	return DELUGE_SUCCESS;
 err_station:
	while ((elem = list_pop(&nlist)) != NULL)
		free_station(list_item(elem, struct station, stqueue));
	i = len;
 err_program:
	while (i-- > 0)
		free_program(&devs[i]->highway);
 err:
	free(devs);
	return err;
}

static struct station *acquire_station(struct deluge_highway *this)
{
	struct list *elem = NULL;

	pthread_mutex_lock(&this->qlock);

	elem = list_pop(&this->stidle);
	if (elem == NULL)
		goto out;

	list_push(&this->stbusy, elem);

 out:
	pthread_mutex_unlock(&this->qlock);

	if (elem == NULL)
		return NULL;
	return list_item(elem, struct station, stqueue);
}

static void release_station(struct deluge_highway *this, struct station *s)
{
	struct list *ejob;
	int idle;

	pthread_mutex_lock(&this->qlock);

	if (this->stopping)
		ejob = NULL;
	else
		ejob = list_pop(&this->jobqueue);

	if (ejob == NULL) {
		list_remove(&s->stqueue);
		list_push(&this->stidle, &s->stqueue);		
	}

	idle = list_empty(&this->stbusy);

	pthread_mutex_unlock(&this->qlock);

	if (this->stopping && idle) {
		finlz_dispatch(this);
		free(this);
	} else if (ejob != NULL) {
		launch_job(s, list_item(ejob, struct job, queue));
	}
}

static void enqueue_job(struct deluge_highway *this, struct job *job)
{
	pthread_mutex_lock(&this->qlock);
	list_push(&this->jobqueue, &job->queue);
	pthread_mutex_unlock(&this->qlock);
}

int deluge_highway_schedule(deluge_highway_t highway, const uint64_t *elems,
			    size_t nelem, void (*cb)(int, uint64_t[5], void *),
			    void *user)
{
	struct station *station;
	struct job *job;
	int err;

	job = malloc(sizeof (*job));
	if (job == NULL) {
		err = deluge_c_error();
		goto err;
	}

	job->input = elems;
	job->ninput = nelem;
	job->user = user;
	job->cb = cb;
	list_init(&job->queue);
	job->dispatch = highway;

	station = acquire_station(highway);
	if (station == NULL) {
		enqueue_job(highway, job);
		goto out;
	}

	launch_job(station, job);
 out:
	return DELUGE_SUCCESS;
 err:
	return err;
}
