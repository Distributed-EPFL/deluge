#ifndef _DELUGE_COMPILE_H_
#define _DELUGE_COMPILE_H_


#include "deluge/opencl.h"


struct device;


struct text_source
{
	const char *name;
	const char *start;
	const char *end;
};

int init_text(cl_program *text, struct device *dev,
	      const struct text_source *headers, size_t nheader,
	      const struct text_source *sources, size_t nsource);


#endif
