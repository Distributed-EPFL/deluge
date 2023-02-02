#ifndef _DELUGE_DELUGE_H_
#define _DELUGE_DELUGE_H_


#include "deluge/opencl.h"
#include <pthread.h>


struct deluge
{
	cl_platform_id   *platforms;                  /* array of platforms */
	size_t            nplatform;                  /* size of `platofrms` */
	struct device    *devices;                    /* array of devices */
	size_t            ndevice;                    /* size of `devices` */
};


struct deluge *get_deluge(int *err);

void put_deluge(void);


#endif
