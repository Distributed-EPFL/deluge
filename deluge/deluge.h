#ifndef _DELUGE_DELUGE_H_
#define _DELUGE_DELUGE_H_


#include "deluge/atomic.h"
#include <stddef.h>


struct device;

struct deluge
{
	atomic_uint64_t   refcnt;
	struct device    *devices;  /* all discovered devices */
	size_t            ndevice;  /* number of discovered devices */
};

struct deluge *retain_deluge(struct deluge *this);

void release_deluge(struct deluge *this);


#endif
