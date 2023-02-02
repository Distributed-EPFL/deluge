#ifndef _DELUGE_DEVICE_H_
#define _DELUGE_DEVICE_H_


#include "deluge/opencl.h"
#include <pthread.h>
#include <stdint.h>


struct deluge;

struct device
{
	struct deluge    *root;              /* deluge context */
	cl_device_id      id;                /* id of OpenCL device */
	cl_context        ctx;               /* device specific context */
	pthread_mutex_t   lock;              /* device lock */
	uint32_t          programs;          /* initialized libraries bitset */
	cl_program        blake3;            /* blake3 library */
};

int init_device(struct device *this, struct deluge *root, cl_device_id id);

void finlz_device(struct device *this);


int init_device_blake3(struct device *this);

cl_program *get_device_blake3(struct device *this, int *err);


#endif
