#ifndef _DELUGE_HASHSUM64_H_
#define _DELUGE_HASHSUM64_H_


#include "deluge/device.h"
#include "deluge/dispatch.h"
#include <pthread.h>


struct hashsum64;

struct hashsum64_backend;

struct hashsum64_job;


struct hashsum64_vtable
{
	int  (*prepare)(struct hashsum64 *, cl_kernel *, struct device *);
	int  (*setup)(struct hashsum64 *, void *, size_t *, struct device *);
	void (*destroy)(struct hashsum64 *);
};

struct hashsum64
{
	struct deluge_dispatch     super;       /* generic hashsum64 logic */
	struct hashsum64_vtable    vtable;      /* hash specific logic */
	struct hashsum64_backend  *backends;    /* device specific backends */
	pthread_mutex_t            joblock;     /* enqueued jobs lock */
	struct hashsum64_job      *jobs;        /* enqueued jobs ring buffer */
	size_t                     jobcap;
	size_t                     jobhead;
	size_t                     jobtail;
};


int init_hashsum64(struct hashsum64 *this, const struct hashsum64_vtable *vt);

void finlz_hashsum64(struct hashsum64 *this);


#endif
