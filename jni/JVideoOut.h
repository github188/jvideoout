#ifndef JVS_VIDEO_OUT_H
#define JVS_VIDEO_OUT_H
/*****************************************************************************
Copyright (C), 2014-2014, Jovision Tech. co., Ltd.
File Name:      JVideoOut.h
Author:         wxy(walker)
date:           2014.08.15			
Description:
android surfaceview opengl/egl displays a frame of yuv420p image
*****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

typedef void * JVO_HANDLE; // vo handle

// yuv plane
typedef struct
{
    unsigned char * p_pixels; // y/u/v data pointer
    int             i_pitch;  // y/u/v pitch
}VO_PLANE;

// vo in
typedef struct
{
    VO_PLANE        p[4];     // y/u/v
    unsigned int    i_chroma; // yuv format not used!
    unsigned int    i_width;  // y width
    unsigned int    i_height; // u width
	int i_visible_width;                 /**< width of visible area */
    int i_visible_height;               /**< height of visible area */
}VO_IN_YUV, *PVO_IN_YUV;

/*****************************************************************************
 *JVO_Open:
 *Create an vo instance.
 *In:     void* NativeWindow // NativeWindow = ANativeWindow_fromSurface
 *return: return a handle to the newly-created instance, or NULL if an error 	  
*****************************************************************************/
JVO_HANDLE JVO_Open(void* NativeWindow);

/*****************************************************************************
 *JVO_Render:
 *displays a frame of yuv420p image
 *In:    JVO_HANDLE h
 *In:    PVO_IN_YUV pic  // yuv420p input
*Return: return 1, if successful, or < 0 if an error occurred 	  
*****************************************************************************/
int JVO_Render(JVO_HANDLE h, PVO_IN_YUV pic);

/*****************************************************************************
 *JVO_Close:
 *Destroy a vo instance.
 *In:     JVO_HANDLE h
 *Return:   	  
*****************************************************************************/
void JVO_Close(JVO_HANDLE h);

/*****************************************************************************
 *JVO_ClearColor:
 *display the specified color image
 *In:     JVO_HANDLE h
 *in:     red/green/blue/alpha
 *Return: return 1, if successful, or < 0 if an error occurred   	  
*****************************************************************************/
int JVO_ClearColor(JVO_HANDLE h,
                             float red, float green, float blue, float alpha);

/*****************************************************************************
 *JVO_ViewPort:
 *set surface rect
 *In:     JVO_HANDLE h
 *in:     
 *Return: return 1, if successful, or < 0 if an error occurred	  
*****************************************************************************/
int JVO_ViewPort(JVO_HANDLE h, int x, int y, int width, int height);


int JVO_SetOffset(JVO_HANDLE h, int off_x, int off_y);
int JVO_SetScale(JVO_HANDLE h, float scale, float x1, float y1, float x2, float y2);
int JVO_Scale_Before(JVO_HANDLE h, float x1, float y1, float x2, float y2);

#ifdef __cplusplus
}
#endif

#endif // JVS_VIDEO_OUT_H
