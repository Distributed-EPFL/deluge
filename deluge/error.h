#ifndef _DELUGE_ERROR_H_
#define _DELUGE_ERROR_H_


#include "deluge/opencl.h"


#define deluge_c_error()  __deluge_c_error(__FILE__, __LINE__)

int __deluge_c_error(const char *filename, int linenum);


#define deluge_pthread_error(_ret)  \
	__deluge_pthread_error(__FILE__, __LINE__, _ret)

int __deluge_pthread_error(const char *filename, int linenum, int ret);


#define deluge_cl_error(_clret)  __deluge_cl_error(__FILE__, __LINE__, _clret)

int __deluge_cl_error(const char *filename, int linenum, cl_int clret);


#define deluge_cl_compile_error(_clret, _sname, _prog, _devid)	      \
	__deluge_cl_compile_error(__FILE__, __LINE__, _clret, _sname, \
				  _prog, _devid)

int __deluge_cl_compile_error(const char *filename, int linenum, cl_int clret,
			      const char *srcname, cl_program prog,
			      cl_device_id devid);


#define deluge_cl_link_error(_clret, _prog, _snames, _srcs, _nsrc, _devid) \
	__deluge_cl_link_error(__FILE__, __LINE__, _clret, _prog,	\
			       _snames, _srcs, _nsrc, _devid)		\

int __deluge_cl_link_error(const char *filename, int linenum, cl_int clret,
			   cl_program prog, const char **srcnames,
			   cl_program *srcs, size_t nsrc, cl_device_id devid);


#endif
