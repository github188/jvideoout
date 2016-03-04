#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <GLES2/gl2.h>
#include <math.h>
#include "vlc_fourcc.h"

#include "opengl.h"
#include "../log.h"

#define PICTURE_PLANE_MAX 3

typedef struct video_format_t
{
    vlc_fourcc_t i_chroma;                 /**< picture chroma */

    unsigned int i_width;                  /**< picture width */
    unsigned int i_height;                              /**< picture height */
    int i_x_offset;               /**< start offset of visible area */
    int i_y_offset;               /**< start offset of visible area */

	int i_visible_width;                 /**< width of visible area */
    int i_visible_height;               /**< height of visible area */

}video_format_t;

#   define GLSL_VERSION "100"
#   define VLCGL_TEXTURE_COUNT 1
#   define VLCGL_PICTURE_MAX 1
#   define PRECISION "precision highp float;"
#   define SUPPORTS_SHADERS
#   define glClientActiveTexture(x)


struct vout_display_opengl_t {
    video_format_t fmt;
    vlc_chroma_description_t *chroma;

    int        tex_target;
    int        tex_format;
    int        tex_internal;
    int        tex_type;

    int        tex_width[PICTURE_PLANE_MAX];
    int        tex_height[PICTURE_PLANE_MAX];

    GLuint     texture[VLCGL_TEXTURE_COUNT][PICTURE_PLANE_MAX];

    GLuint     program;
    GLint      shader[3];
    int        local_count;
    GLfloat    local_value[16];

    int use_multitexture;

    int supports_npot;

    unsigned char *texture_temp_buf;
    int      texture_temp_buf_size;

	float scale_w[PICTURE_PLANE_MAX];
	float scale_h[PICTURE_PLANE_MAX];

	float left[PICTURE_PLANE_MAX];
	float top[PICTURE_PLANE_MAX];
	float right[PICTURE_PLANE_MAX];
	float bottom[PICTURE_PLANE_MAX];

	int frame_count;
};

typedef struct vout_display_opengl_t vout_display_opengl_t;

static inline int HasExtension(const char *apis, const char *api)
{
    size_t apilen = strlen(api);
    while (apis) {
        while (*apis == ' ')
            apis++;
        if (!strncmp(apis, api, apilen) && memchr(" ", apis[apilen], 2))
            return 1;
        apis = strchr(apis, ' ');
    }
    return 0;
}

static inline int GetAlignedSize(unsigned size)
{
    /* Return the smallest larger or equal power of 2 */
    unsigned align = 1 << (8 * sizeof (unsigned) - clz(size));
    return ((align >> 1) == size) ? size : align;
	return size;
}


static void BuildVertexShader(vout_display_opengl_t *vgl,
                              GLint *shader)
{
    /* Basic vertex shader */
    const char *vertexShader =
        "#version " GLSL_VERSION "\n"
        PRECISION
        "varying vec4 TexCoord0,TexCoord1, TexCoord2;"
        "attribute vec4 MultiTexCoord0,MultiTexCoord1,MultiTexCoord2;"
        "attribute vec4 VertexPosition;"
        "void main() {"
        " TexCoord0 = MultiTexCoord0;"
        " TexCoord1 = MultiTexCoord1;"
        " TexCoord2 = MultiTexCoord2;"
        " gl_Position = VertexPosition;"
        "}";

    *shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(*shader, 1, &vertexShader, NULL);
    glCompileShader(*shader);
}

static void BuildYUVFragmentShader(vout_display_opengl_t *vgl,
                                   GLint *shader,
                                   int *local_count,
                                   GLfloat *local_value,
                                   const video_format_t *fmt,
                                   float yuv_range_correction)

{
    /* [R/G/B][Y U V O] from TV range to full range
     * XXX we could also do hue/brightness/constrast/gamma
     * by simply changing the coefficients
     */
    const float matrix_bt601_tv2full[12] = {
        1.164383561643836,  0.0000,             1.596026785714286, -0.874202217873451 ,
        1.164383561643836, -0.391762290094914, -0.812967647237771,  0.531667823499146 ,
        1.164383561643836,  2.017232142857142,  0.0000,            -1.085630789302022 ,
    };
    const float matrix_bt709_tv2full[12] = {
        1.164383561643836,  0.0000,             1.792741071428571, -0.972945075016308 ,
        1.164383561643836, -0.21324861427373,  -0.532909328559444,  0.301482665475862 ,
        1.164383561643836,  2.112401785714286,  0.0000,            -1.133402217873451 ,
    };
    const float (*matrix) = fmt->i_height > 576 ? matrix_bt709_tv2full
                                                : matrix_bt601_tv2full;

    /* Basic linear YUV -> RGB conversion using bilinear interpolation */
    const char *template_glsl_yuv =
        "#version " GLSL_VERSION "\n"
        PRECISION
        "uniform sampler2D Texture0;"
        "uniform sampler2D Texture1;"
        "uniform sampler2D Texture2;"
        "uniform vec4      Coefficient[4];"
        "varying vec4      TexCoord0,TexCoord1,TexCoord2;"

        "void main(void) {"
        " vec4 x,y,z,result;"
        " x  = texture2D(Texture0, TexCoord0.st);"
        " %c = texture2D(Texture1, TexCoord1.st);"
        " %c = texture2D(Texture2, TexCoord2.st);"

        " result = x * Coefficient[0] + Coefficient[3];"
        " result = (y * Coefficient[1]) + result;"
        " result = (z * Coefficient[2]) + result;"
        " gl_FragColor = result;"
        "}";
  //  int swap_uv = fmt->i_chroma == VLC_CODEC_YV12;

    int swap_uv = 1;

    char *code;
    if (asprintf(&code, template_glsl_yuv,
                 swap_uv ? 'z' : 'y',
                 swap_uv ? 'y' : 'z') < 0)
        code = NULL;

    for (int i = 0; i < 4; i++) {
        float correction = i < 3 ? yuv_range_correction : 1.0;
        /* We place coefficient values for coefficient[4] in one array from matrix values.
           Notice that we fill values from top down instead of left to right.*/
        for (int j = 0; j < 4; j++)
            local_value[*local_count + i*4+j] = j < 3 ? correction * matrix[j*4+i]
                                                      : 0.0 ;
    }
    (*local_count) += 4;


    *shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(*shader, 1, (const char **)&code, NULL);
    glCompileShader(*shader);

    free(code);
}

vout_display_opengl_t *vout_display_opengl_New(video_format_t *fmt)
{

    vout_display_opengl_t *vgl = calloc(1, sizeof(*vgl));
    if (!vgl)
        return NULL;

    memset(vgl, 0, sizeof(*vgl));

    float yuv_range_correction = 1.0;

    /* Build program if needed */
    vgl->program = 0;
    vgl->shader[0] =
    vgl->shader[1] =
    vgl->shader[2] = -1;
    vgl->local_count = 0;
  //  if (supports_shaders && (need_fs_yuv || need_fs_xyz|| need_fs_rgba)) {

        BuildYUVFragmentShader(vgl, &vgl->shader[0], &vgl->local_count,
                                vgl->local_value, fmt, yuv_range_correction);
        BuildVertexShader(vgl, &vgl->shader[2]);


        /* Check shaders messages */
        for (unsigned j = 0; j < 3; j++) {
            int infoLength;
            glGetShaderiv(vgl->shader[j], GL_INFO_LOG_LENGTH, &infoLength);
            if (infoLength <= 1)
                continue;

            char *infolog = malloc(infoLength);
            int charsWritten;
            glGetShaderInfoLog(vgl->shader[j], infoLength, &charsWritten, infolog);
            free(infolog);
        }

        vgl->program = glCreateProgram();
        glAttachShader(vgl->program, vgl->shader[0]);
        glAttachShader(vgl->program, vgl->shader[2]);
        glLinkProgram(vgl->program);


        /* Check program messages */
		int infoLength = 0;
		glGetProgramiv(vgl->program, GL_INFO_LOG_LENGTH, &infoLength);
		char *infolog = malloc(infoLength);
		int charsWritten;
		glGetProgramInfoLog(vgl->program, infoLength, &charsWritten, infolog);
		free(infolog);

		/* If there is some message, better to check linking is ok */
		GLint link_status = GL_TRUE;
		glGetProgramiv(vgl->program, GL_LINK_STATUS, &link_status);
		if (link_status == GL_FALSE) {
			LOGI("Unable to use program \n");
			free(vgl);
			return NULL;
		}
//	}


    /* */
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    return vgl;
}

void vout_display_opengl_Delete(vout_display_opengl_t *vgl)
{
	if (vgl == NULL)
	{
	   return;
	}

	glFinish();
	glFlush();
	if (vgl->chroma != NULL)
	{
		for (int i = 0; i < VLCGL_TEXTURE_COUNT; i++)
			glDeleteTextures(vgl->chroma->plane_count, vgl->texture[i]);
	}


	if (vgl->program) {
		glDeleteProgram(vgl->program);
		for (int i = 0; i < 3; i++)
			glDeleteShader(vgl->shader[i]);
	}

	if (vgl->texture_temp_buf != NULL)
	{
		free(vgl->texture_temp_buf);
		vgl->texture_temp_buf = NULL;
	}
	
    free(vgl);

}


#define ALIGN(x, y) (((x) + ((y) - 1)) & ~((y) - 1))
static void Upload(vout_display_opengl_t *vgl, int in_width, int in_height,
                   int in_full_width, int in_full_height,
                   int w_num, int w_den, int h_num, int h_den,
                   int pitch, int pixel_pitch,
                   int full_upload, const uint8_t *pixels,
                   int tex_target, int tex_format, int tex_type)
{
    int width       =       in_width * w_num / w_den;
    int full_width  =  in_full_width * w_num / w_den;
    int height      =      in_height * h_num / h_den;
    int full_height = in_full_height * h_num / h_den;

 //   width = full_width = 352;
 //   height = full_height = 288;
    // This unpack alignment is the default, but setting it just in case.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
#ifndef GL_UNPACK_ROW_LENGTH
    int dst_width = full_upload ? full_width : width;
    int dst_pitch = ALIGN(dst_width * pixel_pitch, 4);

  //  dst_width = dst_pitch = 352;

 //   LOGI("upload11: full_upload: %d, height: %d", full_upload, height);

   // dst_pitch = pitch = width;
    if ( pitch != dst_pitch )
    {
 //   	LOGI("Upload pitch != dst_pitch %d != %d height: %d", pitch, dst_pitch, height);
        int buf_size = dst_pitch * full_height * pixel_pitch;
        const uint8_t *source = pixels;
        uint8_t *destination;
        if( !vgl->texture_temp_buf || vgl->texture_temp_buf_size < buf_size )
        {
            free( vgl->texture_temp_buf );
            vgl->texture_temp_buf = malloc( buf_size );
            vgl->texture_temp_buf_size = buf_size;
        }
        destination = vgl->texture_temp_buf;

        for( int h = 0; h < height ; h++ )
        {
            memcpy( destination, source, width * pixel_pitch );
            source += pitch;
            destination += dst_pitch;
        }
        if (full_upload)
            glTexImage2D( tex_target, 0, tex_format,
                          full_width, full_height,
                          0, tex_format, tex_type, vgl->texture_temp_buf );
        else
            glTexSubImage2D( tex_target, 0,
                             0, 0,
                             width, height,
                             tex_format, tex_type, vgl->texture_temp_buf );
    } else {
#else
    (void) width;
    (void) height;
    (void) vgl;
    {

    	glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / pixel_pitch);
#endif
        if (full_upload)
        {
        	glTexImage2D(tex_target, 0, tex_format,
                         full_width, full_height,
                         0, tex_format, tex_type, pixels);
        }
        else
            glTexSubImage2D(tex_target, 0,
                            0, 0,
                            width, height,
                            tex_format, tex_type, pixels);
    }
    
//    LOGI("opengl pitch Iden two");

}

int vout_display_opengl_Prepare(vout_display_opengl_t *vgl, PVO_IN_YUV picture)
{
    /* Update the texture */
    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        if (vgl->use_multitexture) {
            glActiveTexture(GL_TEXTURE0 + j);
            glClientActiveTexture(GL_TEXTURE0 + j);
        }
        glBindTexture(vgl->tex_target, vgl->texture[0][j]);

        Upload(vgl, vgl->fmt.i_width, vgl->fmt.i_height,
               vgl->fmt.i_width, vgl->fmt.i_height,
               vgl->chroma->p[j].w.num, vgl->chroma->p[j].w.den, vgl->chroma->p[j].h.num, vgl->chroma->p[j].h.den,
               picture->p[j].i_pitch, 1, 0, picture->p[j].p_pixels, vgl->tex_target, vgl->tex_format, vgl->tex_type);
    }

    return 1;
}


static void DrawWithShaders(vout_display_opengl_t *vgl, float *left, float *top, float *right, float *bottom)
{
    glUseProgram(vgl->program);

	if (vgl->chroma->plane_count == 3) {
		glUniform4fv(glGetUniformLocation(vgl->program, "Coefficient"), 4, vgl->local_value);
		glUniform1i(glGetUniformLocation(vgl->program, "Texture0"), 0);
		glUniform1i(glGetUniformLocation(vgl->program, "Texture1"), 1);
		glUniform1i(glGetUniformLocation(vgl->program, "Texture2"), 2);
	}
	else if (vgl->chroma->plane_count == 1) {
		glUniform1i(glGetUniformLocation(vgl->program, "Texture0"), 0);
	}


    static const GLfloat vertexCoord[] = {
            -1.0,  1.0,
            -1.0, -1.0,
             1.0,  1.0,
             1.0, -1.0,
    };

    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        const GLfloat textureCoord[] = {
            left[j],  top[j],
            left[j],  bottom[j],
            right[j], top[j],
            right[j], bottom[j],
        };
        glActiveTexture(GL_TEXTURE0+j);
        glClientActiveTexture(GL_TEXTURE0+j);
        glBindTexture(vgl->tex_target, vgl->texture[0][j]);

        char attribute[20];
        snprintf(attribute, sizeof(attribute), "MultiTexCoord%1d", j);
        glEnableVertexAttribArray(glGetAttribLocation(vgl->program, attribute));
        glVertexAttribPointer(glGetAttribLocation(vgl->program, attribute), 2, GL_FLOAT, 0, 0, textureCoord);
    }
    glActiveTexture(GL_TEXTURE0 + 0);
    glClientActiveTexture(GL_TEXTURE0 + 0);
    glEnableVertexAttribArray(glGetAttribLocation(vgl->program, "VertexPosition"));
    glVertexAttribPointer(glGetAttribLocation(vgl->program, "VertexPosition"), 2, GL_FLOAT, 0, 0, vertexCoord);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

int BuildTexture(vout_display_opengl_t *vgl, video_format_t *fmt)
{
    const char *extensions = (const char *)glGetString(GL_EXTENSIONS);

    int supports_shaders = 1;

    vgl->supports_npot = HasExtension(extensions, "GL_ARB_texture_non_power_of_two") ||
                         HasExtension(extensions, "GL_APPLE_texture_2D_limited_npot");
    vgl->supports_npot = 1;

    GLint max_texture_units = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);
    
    /* Initialize with default chroma */
    vgl->fmt = *fmt;

    vgl->fmt.i_chroma = VLC_CODEC_YV12;
    vgl->tex_target   = GL_TEXTURE_2D;
    vgl->tex_format   = GL_RGBA;
    vgl->tex_internal = GL_RGBA;
    vgl->tex_type     = GL_UNSIGNED_BYTE;


//	LOGI("max_texture_units = %d, supports_shaders: %d",max_texture_units, supports_shaders);

    if (max_texture_units >= 3 && supports_shaders && vlc_fourcc_IsYUV(fmt->i_chroma)) {
        const vlc_fourcc_t *list = vlc_fourcc_GetYUVFallback(fmt->i_chroma);
        while (*list) {
            const vlc_chroma_description_t *dsc = vlc_fourcc_GetChromaDescription(*list);
            if (dsc && dsc->plane_count == 3 && dsc->pixel_size == 1) {
                vgl->fmt          = *fmt;
                vgl->fmt.i_chroma = *list;
                vgl->tex_format   = GL_LUMINANCE;
                vgl->tex_internal = GL_LUMINANCE;
                vgl->tex_type     = GL_UNSIGNED_BYTE;
                break;
            }
            list++;
        }
    }
    

    vgl->chroma = vlc_fourcc_GetChromaDescription(vgl->fmt.i_chroma);
    vgl->use_multitexture = vgl->chroma->plane_count > 1;
   
    /* Texture size */
    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        int w = vgl->fmt.i_width  * vgl->chroma->p[j].w.num / vgl->chroma->p[j].w.den;
        int h = vgl->fmt.i_height * vgl->chroma->p[j].h.num / vgl->chroma->p[j].h.den;
        if (vgl->supports_npot) {
            vgl->tex_width[j]  = w;
            vgl->tex_height[j] = h;
        } else {
            vgl->tex_width[j]  = GetAlignedSize(w);
            vgl->tex_height[j] = GetAlignedSize(h);
        }

    }

    for (int i = 0; i < VLCGL_TEXTURE_COUNT; i++) {
        glGenTextures(vgl->chroma->plane_count, vgl->texture[i]);
        for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
            if (vgl->use_multitexture) {
                glActiveTexture(GL_TEXTURE0 + j);
                glClientActiveTexture(GL_TEXTURE0 + j);
            }
            glBindTexture(vgl->tex_target, vgl->texture[i][j]);


            glTexParameteri(vgl->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(vgl->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(vgl->tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(vgl->tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            /* Call glTexImage2D only once, and use glTexSubImage2D later */
            glTexImage2D(vgl->tex_target, 0,
                         vgl->tex_internal, vgl->tex_width[j], vgl->tex_height[j],
                         0, vgl->tex_format, vgl->tex_type, NULL);
        }
    }

    *fmt = vgl->fmt;

	return 1;
}

typedef struct _OPENGL
{
	video_format_t 				fmt;
	vout_display_opengl_t *		vgl;
	double						distance;
	float						x1;
	float						y1;
	float						x2;
	float						y2;
	float						scale;
	float						off_x;
	float						off_y;
}OPENGL, *POPENGL;




OPENGL_HANDLE opengl_open(int width, int height)
{

	OPENGL_HANDLE h = NULL;

    h = malloc(sizeof(OPENGL));
    if (h == NULL)
    {
    	goto fail;
    }
    memset(h, 0, sizeof(OPENGL));

    memset(&h->fmt, 0, sizeof(video_format_t));
	
	h->scale = 1.0;
    h->vgl = vout_display_opengl_New (&h->fmt);

// 	h->fmt.i_visible_width = width;
// 	h->fmt.i_visible_height = height;

// 	h->vgl->fmt.i_visible_width = width;
// 	h->vgl->fmt.i_visible_height = height;


    return h;

fail:
    opengl_close(h);

	return NULL;
}

float g_scale_w = 0.0;
float g_scale_h = 0.0;
int g_x_offset = 0;
int g_y_offset = 0;

int frame_count = 0;

float g_scale = 0.0;

static double getDistance(OPENGL_HANDLE h, float x1, float y1, float x2, float y2)
{
	return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

int opengl_scale_before(OPENGL_HANDLE h, float x1, float y1, float x2, float y2)
{
	h->x1 = x1;
	h->y1 = y1;
	h->x2 = x2;
	h->y2 = y2;

	h->distance = getDistance(h, x1, y1, x2, y2);

	LOGI("opengl_scale_before: h: 0x%p, dis: %f, x1: %f, y1: %f,x2: %f, y2: %f", h, h->distance, x1, y1, x2, y2);

	return 1;
}

int opengl_set_scale(OPENGL_HANDLE h, float scale1, float x1, float y1, float x2, float y2, 
					 int i_visible_width, int i_visible_height)
{

	double scale = 0.0;
	unsigned int j;

	double cur_dis;

	float off_x, off_y;

	vout_display_opengl_t *		vgl = NULL;
	if ((h == NULL) && (h->vgl == NULL))
	{
		return -1;
	}
	
	vgl = h->vgl;

	cur_dis = getDistance(h, x1, y1, x2, y2);
	scale =   h->distance / cur_dis;

	off_x = fabs((x1 + x2) / 2 - (h->x1 + h->x2) / 2);
	off_y = fabs((y1 + y2) / 2 - (h->y1 + h->y2) / 2);

	h->scale *= scale;
	h->distance = cur_dis;

	h->off_x += off_x;
	h->off_y += off_y;
	
	h->x1 = x1;
	h->y1 = y1;
	h->x2 = x2;
	h->y2 = y2;


	for (j = 0; j < vgl->chroma->plane_count; j++)
	{	
		vgl->left[j]   = h->off_x / i_visible_width;
		vgl->top[j]    = h->off_y / i_visible_height; 
		vgl->right[j]  = vgl->left[j] + h->scale;
		vgl->bottom[j] = vgl->top[j] + h->scale;
	}


	LOGI("scale: %f, i_visible_width: %d, i_visible_height: %d, off_x: %f, off_y: %f, l: %f, t: %f, r: %f, b: %f",
			h->scale, i_visible_width, i_visible_height, h->off_x, h->off_y, 
			vgl->left[0], vgl->top[0], vgl->right[0], vgl->bottom[0]);


	return 1;

}

int opengl_set_offset(OPENGL_HANDLE h, int off_x, int off_y)
{
// 	vout_display_opengl_t *		vgl = NULL;
// 	if ((h == NULL) && (h->vgl == NULL))
// 	{
// 		return -1;
// 	}
// 	
// 	vgl = h->vgl;
// 
// 	h->fmt.i_x_offset -= off_x;
// 	h->fmt.i_y_offset -= off_y;
// 
// 	unsigned int j;
// 	
// 	for (j = 0; j < vgl->chroma->plane_count; j++)
// 	{	
// 		vgl->left[j]   = (h->fmt.i_x_offset +                       0 ) * vgl->scale_w[j];
// 		vgl->top[j]    = (h->fmt.i_y_offset +                       0 ) * vgl->scale_h[j];
// 		vgl->right[j]  = (h->fmt.i_x_offset + h->fmt.i_width ) * vgl->scale_w[j];
// 		vgl->bottom[j] = (h->fmt.i_y_offset + h->fmt.i_height) * vgl->scale_h[j];
// 		
// // 		LOGI("scale_w: %f, scale_h: %f, x: %d, y: %d, l: %f, t: %f, r: %f, b: %f",
// // 			vgl->scale_w[j], vgl->scale_h[j], h->fmt.i_x_offset, h->fmt.i_y_offset,
// // 			vgl->left[j], vgl->top[j], vgl->right[j], vgl->bottom[j]);
// 	}

	return 1;
}

int opengl_do(OPENGL_HANDLE h, PVO_IN_YUV pic)
{

	vout_display_opengl_t *		vgl = NULL;
	if ((h == NULL) && (h->vgl == NULL))
	{
		return -1;
	}

	vgl = h->vgl;
	
	if (h->fmt.i_width != pic->i_width)
	{
	//	LOGI("1 opengl_do h->fmt.i_width ! = pic->i_width %d != %d , BuildTexture",
	//								h->fmt.i_width, pic->i_width);
		h->fmt.i_width = pic->i_width;
		h->fmt.i_height = pic->i_height;
		h->fmt.i_chroma = VLC_CODEC_YV12;
		BuildTexture(h->vgl, &h->fmt);
		
// 		h->fmt.i_x_offset += 10;
// 		h->fmt.i_y_offset += 10;

		unsigned int j;

		for (j = 0; j < vgl->chroma->plane_count; j++)
		{
            vgl->scale_w[j] = (float)vgl->chroma->p[j].w.num / vgl->chroma->p[j].w.den / vgl->tex_width[j];
            vgl->scale_h[j] = (float)vgl->chroma->p[j].h.num / vgl->chroma->p[j].h.den / vgl->tex_height[j];

// 			float scale = 1.2;
// 
// 			vgl->scale_w[j] *= scale;
// 			vgl->scale_h[j] *= scale;

			vgl->left[j]   = (h->fmt.i_x_offset +              0 ) * vgl->scale_w[j];
			vgl->top[j]    = (h->fmt.i_y_offset +              0 ) * vgl->scale_h[j];
			vgl->right[j]  = (h->fmt.i_x_offset + h->fmt.i_width) * vgl->scale_w[j];
			vgl->bottom[j] = (h->fmt.i_y_offset + h->fmt.i_height) * vgl->scale_h[j];
			
			
			g_scale_w = vgl->scale_w[0];
			g_scale_h = vgl->scale_h[0];
		}

// 		LOGI("1scale_w: %f, scale_h: %f, x: %d, y: %d, l: %f, t: %f, r: %f, b: %f",
// 			vgl->scale_w[j], vgl->scale_h[j], h->fmt.i_x_offset, h->fmt.i_y_offset,
// 			vgl->left[0], vgl->top[0], vgl->right[0], vgl->bottom[0]);

    }
	
    vout_display_opengl_Prepare(vgl, pic);

	glClear(GL_COLOR_BUFFER_BIT);

	DrawWithShaders(vgl, vgl->left, vgl->top, vgl->right, vgl->bottom);


	return 1;
}

void opengl_close(OPENGL_HANDLE h)
{
	if (h == NULL)
	{
		return;
	}

	vout_display_opengl_Delete(h->vgl);

	free(h);
}

void opengl_set_view(int left, int top, int width, int height)
{
	glViewport(left, top, width, height);
}

void opengl_clearcolor(float red, float green, float blue, float alpha)
{
	glClearColor(red, green, blue, alpha);
	glClear(GL_COLOR_BUFFER_BIT);
}

