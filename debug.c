#include "debug.h"

#include <stdarg.h>
#include <stdio.h>

#ifdef DEBUG
static pthread_mutex_t debug_io_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

void debug_printf(const char* fmt, ...)
{
#ifdef DEBUG
	pthread_mutex_lock(&debug_io_mutex);

	va_list vl;
	va_start(vl, fmt);
	vprintf(fmt, vl);
	va_end(vl);

	pthread_mutex_unlock(&debug_io_mutex);
#endif
}