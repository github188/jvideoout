#ifndef _OPENGL_H
#define	_OPENGL_H
#include "../JVideoOut.h"

typedef struct _OPENGL*  OPENGL_HANDLE;

OPENGL_HANDLE opengl_open(int width, int height);
int opengl_do(OPENGL_HANDLE h, PVO_IN_YUV pic);
void opengl_close(OPENGL_HANDLE h);

int opengl_scale_before(OPENGL_HANDLE h, float x1, float y1, float x2, float y2);
int opengl_set_scale(OPENGL_HANDLE h, float scale, float x1, float y1, float x2, float y2,
					 int i_visible_width, int i_visible_height);
int opengl_set_offset(OPENGL_HANDLE h, int off_x, int off_y);
void opengl_set_view(int left, int top, int width, int height);
void opengl_clearcolor(float red, float green, float blue, float alpha);

#endif // _OPENGL_H

