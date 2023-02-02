#include <deluge.h>
#include "deluge/error.h"
#include "deluge/opencl.h"


#ifndef NDEBUG


#include <errno.h>
#include <string.h>
#include <stdio.h>


static void print_error_log(const char *srcname, cl_program prog,
			    cl_device_id devid)
{
        cl_int clret;
	size_t size;
	char *buf;

        clret = clGetProgramBuildInfo(prog, devid, CL_PROGRAM_BUILD_LOG,
				      0, NULL, &size);
        if (clret != CL_SUCCESS) {
		deluge_cl_error(clret);
		return;
	}

        if (size == 0)
		return;

        buf = malloc(size + 1);
        if (buf == NULL) {
		deluge_c_error();
		return;
	}

        clret = clGetProgramBuildInfo(prog, devid, CL_PROGRAM_BUILD_LOG,
				      size, buf, NULL);
        if (clret != CL_SUCCESS) {
		free(buf);
		deluge_cl_error(clret);
		return;
	}

        buf[size] = '\0';

	fprintf(stderr, "%s: %s", srcname, buf);

	free(buf);
}


int __deluge_c_error(const char *filename, int linenum)
{
	fprintf(stderr, "%s:%d: %s\n", filename, linenum, strerror(errno));
	return DELUGE_FAILURE;
}

int __deluge_pthread_error(const char *filename, int linenum, int ret)
{
	fprintf(stderr, "%s:%d: %s\n", filename, linenum, strerror(ret));
	return DELUGE_FAILURE;
}

int __deluge_cl_error(const char *filename, int linenum, cl_int clret)
{
	fprintf(stderr, "%s:%d: %s\n", filename, linenum,
		opencl_errstr(clret));
	return DELUGE_FAILURE;
}

int __deluge_cl_compile_error(const char *filename, int linenum, cl_int clret,
			      const char *srcname, cl_program prog,
			      cl_device_id devid)
{
	fprintf(stderr, "%s:%d: %s\n", filename, linenum,
		opencl_errstr(clret));
	if (clret == CL_COMPILE_PROGRAM_FAILURE)
		print_error_log(srcname, prog, devid);
	if (clret == CL_BUILD_PROGRAM_FAILURE)
		print_error_log(srcname, prog, devid);
	return DELUGE_FAILURE;
}

int __deluge_cl_link_error(const char *filename, int linenum, cl_int clret,
			   cl_program prog, const char **srcnames,
			   cl_program *srcs __attribute__ ((unused)),
			   size_t nsrc __attribute__ ((unused)),
			   cl_device_id devid)
{
	fprintf(stderr, "%s:%d: %s\n", filename, linenum,
		opencl_errstr(clret));
	if (clret == CL_LINK_PROGRAM_FAILURE)
		print_error_log(srcnames[0], prog, devid);
	return DELUGE_FAILURE;
}


#else


int __deluge_c_error(const char *filename __attribute__ ((unused)),
		     int linenum __attribute__ ((unused)))
{
	return DELUGE_FAILURE;
}

int __deluge_pthread_error(const char *filename __attribute__ ((unused)),
			   int linenum __attribute__ ((unused)),
			   int ret __attribute__ ((unused)))
{
	return DELUGE_FAILURE;
}

int __deluge_cl_error(const char *filename __attribute__ ((unused)),
		      int linenum __attribute__ ((unused)),
		      cl_int clret __attribute__ ((unused)))
{
	return DELUGE_FAILURE;
}

int __deluge_cl_compile_error(const char *filename __attribute__ ((unused)),
			      int linenum __attribute__ ((unused)),
			      cl_int clret __attribute__ ((unused)),
			      const char *srcname __attribute__ ((unused)),
			      cl_program prog __attribute__ ((unused)),
			      cl_device_id devid __attribute__ ((unused)))
{
	return DELUGE_FAILURE;
}

int __deluge_cl_link_error(const char *filename __attribute__ ((unused)),
			   int linenum __attribute__ ((unused)),
			   cl_int clret __attribute__ ((unused)),
			   cl_program prog __attribute__ ((unused)),
			   const char **srcnames __attribute__ ((unused)),
			   cl_program *srcs __attribute__ ((unused)),
			   size_t nsrc __attribute__ ((unused)),
			   cl_device_id devid __attribute__ ((unused)))
{
	return DELUGE_FAILURE;
}


#endif
