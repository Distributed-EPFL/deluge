#include <deluge.h>
#include "deluge/dispatch.h"


int init_dispatch(struct deluge_dispatch *this,
		  const struct dispatch_vtable *vtable)
{
	int err;

	this->root = get_deluge(&err);
	if (err != DELUGE_SUCCESS)
		goto err;

	this->vtable = *vtable;

	return DELUGE_SUCCESS;
 err:
	return err;
}

void finlz_dispatch(struct deluge_dispatch *this __attribute__ ((unused)))
{
	put_deluge();
}


void deluge_dispatch_destroy(deluge_dispatch_t this)
{
	this->vtable.destroy(this);
}
