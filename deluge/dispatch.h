#ifndef _DELUGE_DISPATCH_H_
#define _DELUGE_DISPATCH_H_


#include "deluge/deluge.h"


struct deluge_dispatch;

struct dispatch_vtable
{
	void (*destroy)(struct deluge_dispatch *);
};

struct deluge_dispatch
{
	struct dispatch_vtable   vtable;   /* job specific logic */
	struct deluge           *root;     /* deluge context */
};


int init_dispatch(struct deluge_dispatch *this,
		  const struct dispatch_vtable *vtable);

void finlz_dispatch(struct deluge_dispatch *this);


#endif
