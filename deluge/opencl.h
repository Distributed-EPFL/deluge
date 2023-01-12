#if defined (__OPENCL_VERSION__)
#  ifndef _DELUGE_OPENCL_H_BIN_
#    define _DELUGE_OPENCL_H_BIN_
#    define __DELUGE_OPENCL_H__
#  endif
#else
#  ifndef _DELUGE_OPENCL_H_
#    define _DELUGE_OPENCL_H_
#    define __DELUGE_OPENCL_H__
#  endif
#endif


#ifdef __DELUGE_OPENCL_H__
#undef __DELUGE_OPENCL_H__


#if defined (__OPENCL_VERSION__)


typedef char  int8_t;
typedef short int16_t;
typedef int   int32_t;
typedef long  int64_t;

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;


#else  /* !defined (__OPENCL_VERSION__) */


#ifndef CL_TARGET_OPENCL_VERSION
#  define CL_TARGET_OPENCL_VERSION 300
#endif


#include <CL/cl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>


const char *opencl_errstr(cl_int ret);


#define kernel
#define global
#define constant
#define local
#define private


#endif  /* !defined (__OPENCL_VERSION__) */


#endif
