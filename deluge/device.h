#ifndef _DELUGE_DEVICE_H_
#define _DELUGE_DEVICE_H_


#include "deluge/highway.h"
#include "deluge/opencl.h"
#include <pthread.h>
#include <stdint.h>


struct deluge;

struct device
{
	struct deluge *root;
	cl_device_id devid;
	cl_context ctx;
	cl_device_type devtype;
	size_t total_gmem;
	size_t total_lmem;
	pthread_mutex_t lock;
	size_t used_gmem;
	size_t used_lmem;
	uint8_t avprogs;
	struct highway_program highway;
};

int init_device(struct device *this, struct deluge *parent, 
		cl_device_id devid);

void finlz_device(struct device *this);


size_t get_device_gmem(struct device *this);

size_t get_device_lmem(struct device *this);

int alloc_on_device(struct device *this, size_t gmem, size_t lmem);

void free_on_device(struct device *this, size_t gmem, size_t lmem);


int has_device_highway(const struct device *this);

int init_device_highway(struct device *this);


#endif
