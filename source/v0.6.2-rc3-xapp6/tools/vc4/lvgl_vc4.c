#include "lvgl_vc4.h"
#include "bcm_host.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <string.h>

static EGLDisplay display = EGL_NO_DISPLAY;
static EGLSurface surface = EGL_NO_SURFACE;
static EGLContext context = EGL_NO_CONTEXT;
static DISPMANX_DISPLAY_HANDLE_T monitor;
static EGL_DISPMANX_WINDOW_T window;
static GLuint texture;
static GLuint spare_texture,scroll_fbo,scroll_program,present_program;
static GLint scroll_delta,scroll_left,scroll_right,scroll_top,scroll_bottom,scroll_height;
static unsigned surface_width,surface_height;
static unsigned output_width,output_height;
static int scroll_supported;
static int active;

#if XDOS_VC4_UPLOAD_DIFF
typedef struct { unsigned left,right; } KnownRow;
static KnownRow *known_rows;
static uint16_t *texture_shadow,*upload_scratch;
static size_t scratch_bytes;
static int diff_frame;
#endif

void xdos_vc4_track_uploads(int enabled)
{
#if XDOS_VC4_UPLOAD_DIFF
    diff_frame=0;
    if (!enabled) {
        free(known_rows);
        free(texture_shadow);
        free(upload_scratch);
        known_rows=NULL;
        texture_shadow=upload_scratch=NULL;
        scratch_bytes=0;
    } else if (active && !known_rows) {
        texture_shadow=malloc((size_t)surface_width*surface_height*sizeof *texture_shadow);
        known_rows=calloc(surface_height,sizeof *known_rows);
        if (!texture_shadow || !known_rows) xdos_vc4_track_uploads(0);
    }
#else
    (void)enabled;
#endif
}

void xdos_vc4_diff_frame(void)
{
#if XDOS_VC4_UPLOAD_DIFF
    diff_frame=known_rows!=NULL;
#endif
}

#if XDOS_VC4_UPLOAD_DIFF
static unsigned upload_changed(unsigned x,unsigned y,unsigned width,unsigned height,
                               const uint16_t *pixels)
{
    unsigned left=0,top=0,right=width,bottom=height;
    if (diff_frame) {
        left=width;
        top=height;
        right=bottom=0;
        for (unsigned row=0;row<height;++row) {
            const KnownRow known=known_rows[y+row];
            const uint16_t *src=pixels+(size_t)row*width;
            const uint16_t *old=texture_shadow+(size_t)(y+row)*surface_width+x;
            unsigned first=0,last=width;
            if (x>=known.left && x+width<=known.right) {
                if (!memcmp(src,old,width*sizeof *src)) continue;
                while (last-first>32 && !memcmp(src+first,old+first,32*sizeof *src)) first+=32;
                while (last-first>32 && !memcmp(src+last-32,old+last-32,32*sizeof *src)) last-=32;
            }
            if (first<left) left=first;
            if (last>right) right=last;
            if (row<top) top=row;
            bottom=row+1;
        }
        if (top==height) return 0;
    }

    const uint16_t *upload=pixels+(size_t)top*width;
    if (right-left!=width) {
        const size_t bytes=(size_t)(right-left)*(bottom-top)*sizeof *pixels;
        if (bytes>scratch_bytes) {
            void *buffer=realloc(upload_scratch,bytes);
            if (buffer) {
                upload_scratch=buffer;
                scratch_bytes=bytes;
            }
        }
        if (bytes<=scratch_bytes) {
            for (unsigned row=top;row<bottom;++row)
                memcpy(upload_scratch+(size_t)(row-top)*(right-left),
                       pixels+(size_t)row*width+left,(right-left)*sizeof *pixels);
            upload=upload_scratch;
        } else {
            left=0;
            right=width;
        }
    }
    glTexSubImage2D(GL_TEXTURE_2D,0,x+left,y+top,right-left,bottom-top,
                    GL_RGB,GL_UNSIGNED_SHORT_5_6_5,upload);
    // Everything outside the upload rectangle was compared against known pixels.
    for (unsigned row=0;row<height;++row) {
        KnownRow *known=&known_rows[y+row];
        memcpy(texture_shadow+(size_t)(y+row)*surface_width+x,
               pixels+(size_t)row*width,width*sizeof *pixels);
        if (known->right<=known->left || x>known->right || x+width<known->left) {
            known->left=x;
            known->right=x+width;
        } else {
            if (x<known->left) known->left=x;
            if (x+width>known->right) known->right=x+width;
        }
    }
    return (right-left)*(bottom-top)*sizeof *pixels;
}
#endif

void xdos_vc4_stop(void)
{
    xdos_vc4_track_uploads(0);
    active=0;
    scroll_supported=0;
    if (display!=EGL_NO_DISPLAY) {
        eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        if (surface!=EGL_NO_SURFACE) eglDestroySurface(display,surface);
        if (context!=EGL_NO_CONTEXT) eglDestroyContext(display,context);
        eglTerminate(display);
    }
    display=EGL_NO_DISPLAY;
    surface=EGL_NO_SURFACE;
    context=EGL_NO_CONTEXT;
    if (window.element) {
        DISPMANX_UPDATE_HANDLE_T update=vc_dispmanx_update_start(0);
        if (update) {
            vc_dispmanx_element_remove(update,window.element);
            vc_dispmanx_update_submit_sync(update);
        }
        window.element=0;
    }
    if (monitor) vc_dispmanx_display_close(monitor);
    monitor=0;
}

static GLuint shader(GLenum type,const char *text)
{
    GLuint id=glCreateShader(type);
    GLint compiled=0;
    glShaderSource(id,1,&text,0);
    glCompileShader(id);
    glGetShaderiv(id,GL_COMPILE_STATUS,&compiled);
    if (!compiled) {
        glDeleteShader(id);
        return 0;
    }
    return id;
}

static GLuint create_program(const char *vertex_text,const char *fragment_text)
{
    const GLuint vertex=shader(GL_VERTEX_SHADER,vertex_text);
    const GLuint fragment=shader(GL_FRAGMENT_SHADER,fragment_text);
    if (!vertex || !fragment) return 0;
    const GLuint program=glCreateProgram();
    glAttachShader(program,vertex);
    glAttachShader(program,fragment);
    glBindAttribLocation(program,0,"pos");
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked=0;
    glGetProgramiv(program,GL_LINK_STATUS,&linked);
    if (!linked) {
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

static void init_texture(GLuint *target,unsigned width,unsigned height)
{
    glGenTextures(1,target);
    glBindTexture(GL_TEXTURE_2D,*target);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,width,height,0,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,0);
}

static void setup_scroll(unsigned width,unsigned height)
{
    scroll_program=create_program(
        "attribute vec2 pos; varying vec2 uv;"
        "void main(){gl_Position=vec4(pos,0.,1.);uv=(pos+1.)*.5;}",
        "precision highp float; varying vec2 uv; uniform sampler2D pixels;"
        "uniform float dy,left,right,top,bottom,height;"
        "void main(){vec2 at=uv;float y=gl_FragCoord.y;"
        "if(gl_FragCoord.x>=left && gl_FragCoord.x<right && y>=top && y<bottom && y+dy>=top && y+dy<bottom)"
        "at.y+=dy/height;gl_FragColor=texture2D(pixels,at);}");
    if (!scroll_program) return;
    glUseProgram(scroll_program);
    glUniform1i(glGetUniformLocation(scroll_program,"pixels"),0);
    scroll_delta=glGetUniformLocation(scroll_program,"dy");
    scroll_left=glGetUniformLocation(scroll_program,"left");
    scroll_right=glGetUniformLocation(scroll_program,"right");
    scroll_top=glGetUniformLocation(scroll_program,"top");
    scroll_bottom=glGetUniformLocation(scroll_program,"bottom");
    scroll_height=glGetUniformLocation(scroll_program,"height");
    glUniform1f(scroll_height,height);
    init_texture(&spare_texture,width,height);
    glGenFramebuffers(1,&scroll_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER,scroll_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,spare_texture,0);
    scroll_supported=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glUseProgram(present_program);
    glBindTexture(GL_TEXTURE_2D,texture);
    if (glGetError()!=GL_NO_ERROR) scroll_supported=0;
}

static int setup(unsigned width,unsigned height)
{
    static const EGLint config_attributes[]={
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,
        EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_NONE
    };
    static const EGLint context_attributes[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    EGLConfig config;
    EGLint count;
    display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display==EGL_NO_DISPLAY || !eglInitialize(display,0,0) ||
        !eglChooseConfig(display,config_attributes,&config,1,&count) || count<1 ||
        !eglBindAPI(EGL_OPENGL_ES_API)) return 0;
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,context_attributes);
    if (context==EGL_NO_CONTEXT) return 0;

    uint32_t physical_width,physical_height;
    if (graphics_get_display_size(0,&physical_width,&physical_height)<0) return 0;
    output_width=physical_width;
    output_height=physical_height;
    monitor=vc_dispmanx_display_open(0);
    if (!monitor) return 0;
    DISPMANX_UPDATE_HANDLE_T update=vc_dispmanx_update_start(0);
    if (!update) return 0;
    VC_RECT_T destination={0,0,physical_width,physical_height};
    VC_RECT_T source={0,0,width<<16,height<<16};
    window.element=vc_dispmanx_element_add(update,monitor,2,&destination,0,&source,
                                           DISPMANX_PROTECTION_NONE,0,0,0);
    window.width=width;
    window.height=height;
    if (vc_dispmanx_update_submit_sync(update)!=0 || !window.element) return 0;
    surface=eglCreateWindowSurface(display,config,&window,0);
    if (surface==EGL_NO_SURFACE || !eglMakeCurrent(display,surface,surface,context) ||
        !eglSwapInterval(display,1)) return 0;

    present_program=create_program(
        "attribute vec2 pos; varying vec2 uv;"
        "void main(){gl_Position=vec4(pos,0.,1.);uv=(pos+1.)*.5;uv.y=1.-uv.y;}",
        "precision mediump float; varying vec2 uv; uniform sampler2D pixels;"
        "void main(){gl_FragColor=texture2D(pixels,uv);}");
    if (!present_program) return 0;
    glUseProgram(present_program);
    glUniform1i(glGetUniformLocation(present_program,"pixels"),0);
    static const GLfloat corners[]={-1,-1,1,-1,-1,1,1,1};
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,corners);
    glViewport(0,0,width,height);

    glPixelStorei(GL_UNPACK_ALIGNMENT,2);
    init_texture(&texture,width,height);
    if (glGetError()!=GL_NO_ERROR) return 0;
    surface_width=width;
    surface_height=height;
    setup_scroll(width,height);
    return 1;
}

int xdos_vc4_start(unsigned width,unsigned height)
{
    bcm_host_init();
    active=setup(width,height);
    if (!active) xdos_vc4_stop();
    return active;
}

int xdos_vc4_active(void)
{
    return active;
}

void xdos_vc4_get_geometry(unsigned *width,unsigned *height,
                           unsigned *hdmi_width,unsigned *hdmi_height)
{
    *width=active ? surface_width : 0;
    *height=active ? surface_height : 0;
    *hdmi_width=active ? output_width : 0;
    *hdmi_height=active ? output_height : 0;
}

int xdos_vc4_shift_scroll(int delta,unsigned right,unsigned top)
{
    return xdos_vc4_shift_viewport(delta,0,top,right,surface_height);
}

int xdos_vc4_shift_viewport(int delta,unsigned left,unsigned top,unsigned right,unsigned bottom)
{
    if (!active || !scroll_supported || !delta || left>=right || top>=bottom ||
        right>surface_width || bottom>surface_height) return 0;
    glBindFramebuffer(GL_FRAMEBUFFER,scroll_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,spare_texture,0);
    glUseProgram(scroll_program);
    glUniform1f(scroll_delta,delta);
    glUniform1f(scroll_left,left);
    glUniform1f(scroll_right,right);
    glUniform1f(scroll_top,top);
    glUniform1f(scroll_bottom,bottom);
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glUseProgram(present_program);
    if (glGetError()!=GL_NO_ERROR) {
        scroll_supported=0;
        return 0;
    }
    const GLuint old=texture;
    texture=spare_texture;
    spare_texture=old;
    glBindTexture(GL_TEXTURE_2D,texture);
#if XDOS_VC4_UPLOAD_DIFF
    if (known_rows) memset(known_rows+top,0,(bottom-top)*sizeof *known_rows);
#endif
    return 1;
}

int xdos_vc4_upload(unsigned x,unsigned y,unsigned width,unsigned height,
                    const void *rgb565,int last,XdosVc4Timing *timing)
{
    if (!active) return 0;
    const unsigned start=xdos_vc4_clock_us();
#if XDOS_VC4_UPLOAD_DIFF
    if (known_rows) timing->uploaded_bytes=upload_changed(x,y,width,height,rgb565);
    else
#endif
    {
        glTexSubImage2D(GL_TEXTURE_2D,0,x,y,width,height,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,rgb565);
        timing->uploaded_bytes=width*height*2;
    }
    const unsigned uploaded=xdos_vc4_clock_us();
    timing->upload_us=uploaded-start;
    timing->draw_us=timing->swap_us=0;
    if (last) {
#if XDOS_VC4_UPLOAD_DIFF
        diff_frame=0;
#endif
        glDrawArrays(GL_TRIANGLE_STRIP,0,4);
        glFinish();
        const unsigned drawn=xdos_vc4_clock_us();
        if (!eglSwapBuffers(display,surface)) return -1;
        timing->draw_us=drawn-uploaded;
        timing->swap_us=xdos_vc4_clock_us()-drawn;
    }
    return glGetError()==GL_NO_ERROR ? 1 : -1;
}
