#include <deluge.h>
#include "deluge/compile.h"
#include "deluge/device.h"
#include "deluge/error.h"


#ifndef CLFLAGS
#  define CLFLAGS   "-Werror -cl-std=CL3.0"
#endif


static int init_text_source(struct device *dev, const char **name,
			    cl_program *program, const struct text_source *src)
{
	const char *text;
	cl_int clret;
	size_t size;

	if (name != NULL)
		*name = src->name;

	text = src->start;
	size = src->end - text;
	*program = clCreateProgramWithSource(dev->ctx, 1,&text, &size, &clret);

	if (clret != CL_SUCCESS)
		return deluge_cl_error(clret);

	return DELUGE_SUCCESS;
}

int init_text(cl_program *text, struct device *dev,
	      const struct text_source *headers, size_t nheader,
	      const struct text_source *sources, size_t nsource)
{
	const char **header_names;
	const char **source_names;
	cl_program *header_progs;
	cl_program *source_progs;
	cl_int clret;
	size_t i;
	int err;

	header_names = malloc(nheader * sizeof (*header_names));
	if (header_names == NULL) {
		err = deluge_c_error();
		goto err;
	}

	source_names = malloc(nsource * sizeof (*source_names));
	if (source_names == NULL) {
		err = deluge_c_error();
		goto err_header_names;
	}

	header_progs = malloc(nheader * sizeof (*header_progs));
	if (header_progs == NULL) {
		err = deluge_c_error();
		goto err_source_names;
	}

	source_progs = malloc(nsource * sizeof (*source_progs));
	if (source_progs == NULL) {
		err = deluge_c_error();
		goto err_header_progs;
	}

	for (i = 0; i < nheader; i++) {
		err = init_text_source(dev, &header_names[i], &header_progs[i],
				       &headers[i]);
		if (err != DELUGE_SUCCESS)
			goto err_headers;
	}

	for (i = 0; i < nsource; i++) {
		err = init_text_source(dev, &source_names[i], &source_progs[i],
				       &sources[i]);
		if (err != DELUGE_SUCCESS)
			goto err_sources;
	}

	for (i = 0; i < nsource; i++) {
		clret = clCompileProgram(source_progs[i], 1, &dev->id, CLFLAGS,
					 nheader, header_progs, header_names,
					 NULL, NULL);
		if (clret != CL_SUCCESS) {
			err = deluge_cl_compile_error(clret, source_names[i],
						      source_progs[i],
						      dev->id);
			goto err_all_sources;
		}
	}

	*text = clLinkProgram(dev->ctx, 1, &dev->id, NULL, nsource,
			      source_progs, NULL, NULL, &clret);
	if (clret != CL_SUCCESS) {
		err = deluge_cl_link_error(clret, *text, source_names,
					   source_progs, nsource, dev->id);
		goto err_all_sources;
	}

	for (i = 0; i < nsource; i++)
		clReleaseProgram(source_progs[i]);
	for (i = 0; i < nheader; i++)
		clReleaseProgram(header_progs[i]);

	free(source_progs);
	free(header_progs);
	free(source_names);
	free(header_names);

	return DELUGE_SUCCESS;
 err_all_sources:
	i = nsource;
 err_sources:
	while (i-- > 0)
		clReleaseProgram(source_progs[i]);
	i = nheader;
 err_headers:
	while (i-- > 0)
		clReleaseProgram(header_progs[i]);
	free(source_progs);
 err_header_progs:
	free(header_progs);
 err_source_names:
	free(source_names);
 err_header_names:
	free(header_names);
 err:
	return err;
}
