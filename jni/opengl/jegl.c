#include <stdio.h>
#include <malloc.h>
#include <string.h>

#define  EGL_EGLEXT_PROTOTYPES

#include <EGL/egl.h> // requires ndk r5 or newer
#include <EGL/eglext.h>
#include "jegl.h"
#include "../log.h"


typedef struct _EGL
{
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
}EGL, *PEGL;

struct gl_api
{
   const char name[10];
   EGLenum    api;
   EGLint     min_minor;
   EGLint     render_bit;
   EGLint     attr[3];
};

void egl_set_rect(EGL_HANDLE h, int width1, int height1)
{
	EGLint width = 0;
    EGLint height = 0;
	
	const char * egl_extensions = NULL; 
	
	
	if ((h == NULL) || (h->display == NULL) || ( h->surface == NULL))
	{
		return;
	}
	
	if (!eglQuerySurface(h->display, h->surface, EGL_WIDTH, &width) ||
        !eglQuerySurface(h->display, h->surface, EGL_HEIGHT, &height)) {
        LOGI("eglQuerySurface() returned error %d", eglGetError());
		//  goto fail;
    }
	
	LOGI("eglQuerySurface: width: %d, height: %d", width, height);
	
	egl_extensions = eglQueryString(h->display, EGL_EXTENSIONS);
	
	LOGI ( "EGL informations:" ) ; 
	//   LOGI ( "# of configs: %d" , numConfigs ) ; 
    LOGI ( "vendor : %s" , eglQueryString ( h->display , EGL_VENDOR ) ) ; 
    LOGI ( "version : %s" , eglQueryString ( h->display , EGL_VERSION ) ) ; 
    LOGI ( "extensions : %s" , egl_extensions ); 
	LOGI ( "Client API : %s" , eglQueryString ( h->display , EGL_CLIENT_APIS ) ? : "Not Supported" ) ; 
	//   LOGI ( "EGLSurface : %d-%d-%d-%d, config=%p" , r , g , b , a , config ) ; 
	
	// 	if (eglSetSwapRectangleANDROID(h->display, h->surface, 0 , 0 , width , height) == EGL_TRUE) 
	// 	{ 
	// 		// This could fail if this extension is not supported by this 
	// 		// specific surface (of config) 
	// 		LOGI("eglSetSwapRectangleANDROID success");
	//     } 
}

EGL_HANDLE egl_open(void* NativeWindow)
{

	PEGL	h = NULL;

    static const struct gl_api api = {
        "OpenGL_ES", EGL_OPENGL_ES_API, 3, EGL_OPENGL_ES2_BIT,
        { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE },
    };

    const EGLint attribs[] = {
        EGL_RED_SIZE, 5,
        EGL_GREEN_SIZE, 5,
        EGL_BLUE_SIZE, 5,
        EGL_RENDERABLE_TYPE, api.render_bit,
        EGL_NONE
    };



    EGLDisplay display;
    EGLConfig config;
    EGLint numConfigs;
    EGLint format;
    EGLSurface surface;
    EGLContext context;
    EGLint width;
    EGLint height;

    if (NativeWindow == NULL)
    {
    	goto fail;
    }

    h = malloc(sizeof(EGL));
    if (h == NULL)
    {
    	goto fail;
    }

    memset(h, 0, sizeof(EGL));

    if ((display = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
        LOGI("eglGetDisplay() returned error %d", eglGetError());
        goto fail;
    }
    if (!eglInitialize(display, 0, 0)) {
        LOGI("eglInitialize() returned error %d", eglGetError());
        goto fail;
    }

    if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs)) {
        LOGI("eglChooseConfig() returned error %d", eglGetError());
        goto fail;
    }

    if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format)) {
        LOGI("eglGetConfigAttrib() returned error %d", eglGetError());
        goto fail;
    }

    ANativeWindow_setBuffersGeometry(NativeWindow, 0, 0, format);

    if (!(surface = eglCreateWindowSurface(display, config, NativeWindow, 0))) {
        LOGI("eglCreateWindowSurface() returned error %d", eglGetError());
        goto fail;
    }

    if (eglBindAPI (api.api) != EGL_TRUE)
    {
        LOGI("cannot bind EGL API");
        goto fail;
    }

    if (!(context = eglCreateContext(display, config, EGL_NO_CONTEXT, api.attr))) {
        LOGI("eglCreateContext() returned error %d", eglGetError());
        goto fail;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOGI("eglMakeCurrent() returned error %d", eglGetError());
        goto fail;
    }

    if (!eglQuerySurface(display, surface, EGL_WIDTH, &width) ||
        !eglQuerySurface(display, surface, EGL_HEIGHT, &height)) {
        LOGI("eglQuerySurface() returned error %d", eglGetError());
        goto fail;
    }


    h->display = display;
    h->surface = surface;
    h->context = context;

//    LOGI("egl_open success  width: %d, height: %d", width, height);

//	egl_set_rect(h, 352, 288);
    return h;

fail:
    egl_close(h);
    return  NULL;
}


int egl_do(EGL_HANDLE h)
{
	if (h == NULL)
	{
		return -1;
	}

	if (!eglSwapBuffers(h->display, h->surface)) {
	//	LOGI("eglSwapBuffers() returned error %d", eglGetError());
		return -1;
	}


	return 1;

}

void egl_close(EGL_HANDLE h)
{
	if (h == NULL)
	{
		return;
	}

    eglMakeCurrent(h->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    eglDestroyContext(h->display, h->context);

    eglDestroySurface(h->display, h->surface);

    eglTerminate(h->display);

    h->display = EGL_NO_DISPLAY;
    h->surface = EGL_NO_SURFACE;
    h->context = EGL_NO_CONTEXT;
}

void egl_query_surface(EGL_HANDLE h, int * width, int *height)
{
	if ((h == NULL) || (h->display == NULL) || ( h->surface == NULL))
	{
		return;
	}
	
	if (!eglQuerySurface(h->display, h->surface, EGL_WIDTH, width) ||
        !eglQuerySurface(h->display, h->surface, EGL_HEIGHT, height)) {
      //  LOGI("eglQuerySurface() returned error %d", eglGetError());
    }
}

