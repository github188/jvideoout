#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include "opengl/jegl.h"
#include "opengl/opengl.h"
#include "JVideoOut.h"
#include "log.h"

typedef struct  
{
	int 	left;
	int     top;
	int		width;
	int		height;
}vo_rect;

typedef struct _VO_HANDLE_
{
	EGL_HANDLE 		egl;
	OPENGL_HANDLE	opengl;
	vo_rect			set_rect;
	vo_rect			default_rect;
	int				brect;
}VO_HANDLE, *PVO_HANDLE;


JVO_HANDLE JVO_Open(void* NativeWindow)
{
	PVO_HANDLE 	vo 		= NULL;
	EGL_HANDLE 			egl 	= NULL;
	OPENGL_HANDLE		opengl  = NULL;

	if (NativeWindow == NULL)
	{
		LOGI("NativeWindow == NULL");
		goto fail;
	}

	vo = malloc(sizeof(VO_HANDLE));
	if (vo == NULL)
	{
		goto fail;
	}

	memset(vo, 0, sizeof(VO_HANDLE));

	egl = egl_open(NativeWindow);
	if (egl == NULL)
	{
		LOGI("elg_open fail");
		goto fail;
	}

	egl_query_surface(vo->egl, &vo->default_rect.width, &vo->default_rect.height);
// 	vo->set_rect->width = vo->default_rect->width;
// 	vo->set_rect->height = vo->default_rect->height;

	opengl = opengl_open(vo->default_rect.width, vo->default_rect.height);
	if (opengl == NULL)
	{
		LOGI("opengl_open fail");
		goto fail;
	}

	vo->egl = egl;
	vo->opengl = opengl;

	LOGI("JVO_Open success");

    return vo;

fail:
	LOGI("JVO_Open fail");

   JVO_Close(vo);
   return NULL;

}

void JVO_Close(JVO_HANDLE h)
{
	PVO_HANDLE vo = h;

	if (vo == NULL)
	{
		return;
	}

	opengl_close(vo->opengl);
    egl_close(vo->egl);

	free(vo);

	LOGI("JVO_Close success");
}

int JVO_Render(JVO_HANDLE h, PVO_IN_YUV pic)
{
	int left = 0;
	int top = 0;
	int width = 0;
 	int height = 0;

	PVO_HANDLE vo = h;

	if (vo == NULL)
	{
		return -1;
	}

	egl_query_surface(vo->egl, &width, &height);

	pic->i_visible_width = width;
	pic->i_visible_height = height;
	opengl_do(vo->opengl, pic);

	
	if ((vo->default_rect.width != width) || (vo->default_rect.height != height) || 
		vo->brect != 0)
	{
		vo->default_rect.width = width;
		vo->default_rect.height = height;
		if (vo->brect != 0)
		{
			left = vo->set_rect.left;
			top = vo->set_rect.top;
			width = vo->set_rect.width;
			height = vo->set_rect.height;
// 			vo->brect = 0;
// 			left = 0;
// 			top = height - 100;
// 			width = 100;
// 			height = 100;
		}

		opengl_set_view(left, top, width, height);

		LOGI("JVO_Render: width: %d, height: %d", width, height);

	}

	egl_do(vo->egl);

	return 1;
}

int JVO_Scale_Before(JVO_HANDLE h, float x1, float y1, float x2, float y2)
{
	PVO_HANDLE vo = h;
	
	if (vo == NULL)
	{
		return -1;
	}

	return opengl_scale_before(vo->opengl, x1, y1, x2, y2);
}

int JVO_SetScale(JVO_HANDLE h, float scale, float x1, float y1, float x2, float y2)
{
	PVO_HANDLE vo = h;
	
	if (vo == NULL)
	{
		return -1;
	}
	
	return opengl_set_scale(vo->opengl, scale, x1, y1, x2, y2, vo->default_rect.width, vo->default_rect.height);
}

int JVO_SetOffset(JVO_HANDLE h, int off_x, int off_y)
{
	PVO_HANDLE vo = h;
	
	if (vo == NULL)
	{
		return -1;
	}

	return opengl_set_offset(vo->opengl, off_x, off_y);
}

int JVO_ClearColor(JVO_HANDLE h, float red, float green, float blue, float alpha)
{
	PVO_HANDLE vo = h;

	if (vo == NULL)
	{
		return -1;
	}

	opengl_clearcolor(red, green, blue, alpha);
	egl_do(vo->egl);

	return 1;
}

int JVO_ViewPort(JVO_HANDLE h, int x, int y, int width,int height)
{
	PVO_HANDLE vo = h;
	
	if (vo == NULL)
	{
		return -1;
	}
	
	LOGI("JVO_ViewPort1: left: %d, top: %d, width: %d, height: %d", x, y, width, height);

	vo->set_rect.left = x;
	vo->set_rect.top = y;
	vo->set_rect.width = width;
	vo->set_rect.height = height;
	vo->brect = 1;

// 	opengl_set_view(x, y, width, height);
// 	egl_do(vo->egl);

	
	return 1;
}