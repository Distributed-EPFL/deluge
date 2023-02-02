#ifndef _DELUGE_UTIL_H_
#define _DELUGE_UTIL_H_


#define ARRAY_SIZE(_arr)  (sizeof (_arr) / sizeof (*(_arr)))


#define container_of(_ptr, _type, _field)				\
	((_type *) (((void *) _ptr) - __builtin_offsetof(_type, _field)))


#endif
