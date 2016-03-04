
#if (defined WIN32) || (defined LINUX)
#include "log.h"

#ifdef WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

int LOGI(char *fmt, ...)
{
	char	sz[1024]	= {0};
	va_list ap			= NULL;
	
	va_start(ap, fmt);
	vsprintf(sz, fmt, ap);
	va_end(ap);
	
	sz[strlen(sz)] = '\n';
	sz[strlen(sz) + 1] = '\0';

#ifdef WIN32	
	OutputDebugString(sz);
#else
	printf(sz);
#endif
	
	return 1;
}

#define LOGD LOGI
#define LOGE LOGI

#endif
