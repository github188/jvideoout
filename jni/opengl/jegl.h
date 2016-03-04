#ifndef _EGL_H
#define _EGL_H


typedef struct _EGL * EGL_HANDLE;

EGL_HANDLE egl_open(void* NativeWindow);
int egl_do(EGL_HANDLE h);
void egl_close(EGL_HANDLE h);
void egl_query_surface(EGL_HANDLE h, int * width, int * height);


#endif // _EGL_H
