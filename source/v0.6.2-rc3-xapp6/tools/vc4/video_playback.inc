#include <stdlib.h>
#include "video_transport.h"
#include "h264dec.h"

enum { VideoWidth=320, VideoHeight=240, VideoFrameBytes=VideoWidth*VideoHeight*3/2 };
static uint32_t video_ready_frame;

static void video_output(uint32_t address,int64_t pts,bool eos)
{
    (void)pts;
    if (!eos && address) video_ready_frame=address;
}

static GLuint video_shader(GLenum type,const char *source)
{
    GLuint shader=glCreateShader(type);
    glShaderSource(shader,1,&source,NULL);
    glCompileShader(shader);
    GLint ok=0;
    glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static bool video_graphics(GLuint textures[3])
{
    GLuint v=video_shader(GL_VERTEX_SHADER,
        "attribute vec2 pos; varying vec2 uv;"
        "void main(){gl_Position=vec4(pos,0.,1.);uv=vec2(pos.x+1.,1.-pos.y)*.5;}");
    GLuint f=video_shader(GL_FRAGMENT_SHADER,
        "precision mediump float; varying vec2 uv; uniform sampler2D yy,uu,vv;"
        "void main(){float y=1.164*(texture2D(yy,uv).r-.0625);"
        "float u=texture2D(uu,uv).r-.5;float v=texture2D(vv,uv).r-.5;"
        "gl_FragColor=vec4(y+1.596*v,y-.392*u-.813*v,y+2.017*u,1.);}");
    if (!v || !f) return false;
    GLuint program=glCreateProgram();
    glAttachShader(program,v);
    glAttachShader(program,f);
    glBindAttribLocation(program,0,"pos");
    glLinkProgram(program);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok=0;
    glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if (!ok) return false;
    glUseProgram(program);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glGenTextures(3,textures);
    const char *names[]={"yy","uu","vv"};
    for (unsigned plane=0;plane<3;++plane) {
        const unsigned shift=plane!=0;
        glUniform1i(glGetUniformLocation(program,names[plane]),plane);
        glActiveTexture(GL_TEXTURE0+plane);
        glBindTexture(GL_TEXTURE_2D,textures[plane]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE,VideoWidth>>shift,VideoHeight>>shift,0,
                     GL_LUMINANCE,GL_UNSIGNED_BYTE,NULL);
    }
    static const GLfloat positions[]={-1,-1,1,-1,-1,1,1,1};
    glBindBuffer(GL_ARRAY_BUFFER,0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,positions);
    glEnableVertexAttribArray(0);
    return eglSwapInterval(state->display,1) && glGetError()==GL_NO_ERROR;
}

static void video_prepare(const GLuint textures[3],uint32_t address)
{
    xdos_video_cache_sync(address,VideoFrameBytes);
    const unsigned sizes[]={VideoWidth*VideoHeight,VideoWidth*VideoHeight/4,VideoWidth*VideoHeight/4};
    const uint8_t *data=(const uint8_t *)(uintptr_t)address;
    for (unsigned plane=0;plane<3;++plane) {
        const unsigned shift=plane!=0;
        glActiveTexture(GL_TEXTURE0+plane);
        glBindTexture(GL_TEXTURE_2D,textures[plane]);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,VideoWidth>>shift,VideoHeight>>shift,
                        GL_LUMINANCE,GL_UNSIGNED_BYTE,data);
        data+=sizes[plane];
    }
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
}

static bool video_swap(void)
{
    return eglSwapBuffers(state->display,state->surface) && glGetError()==GL_NO_ERROR;
}

static unsigned video_find_aud(const uint8_t *file,unsigned bytes,unsigned from)
{
    for (unsigned i=from;i+3<bytes;++i)
        if (!file[i] && !file[i+1] && file[i+2]==1 && (file[i+3]&31)==9)
            return i>from && !file[i-1] ? i-1 : i;
    return bytes;
}

static void video_playback(void)
{
    unsigned size=xdos_video_size();
    if (!size || size>4u*1024*1024) {
        xdos_video_log("[H264] Missing or oversized SD test clip");
        return;
    }
    uint8_t *file=malloc(size);
    if (!file || xdos_video_read(file,size)!=(int)size) {
        xdos_video_log("[H264] SD read failed");
        free(file);
        return;
    }
    GLuint textures[3];
    if (!video_graphics(textures) || !h264dec_init(VideoWidth,VideoHeight,video_output)) {
        xdos_video_log("[H264] Could not start VC4 playback");
        free(file);
        return;
    }
    uint32_t handles[3]={0};
    unsigned shown=0;
    for (unsigned i=0;i<3;++i) {
        uint32_t address=vchiq_alloc_shared(VideoFrameBytes,&handles[i]);
        if (!address || !h264dec_add_output_buffer(address,VideoFrameBytes)) {
            xdos_video_log("[H264] Cannot register decoder output %u",i);
            goto done;
        }
    }
    xdos_video_log("[H264] Playing 320x240 Annex-B from SD");
    const unsigned started=vc4_clock_us();
    for (unsigned at=video_find_aud(file,size,0);at<size;) {
        unsigned end=video_find_aud(file,size,at+5);
        uint32_t capacity=0;
        uint8_t *input=NULL;
        unsigned timeout=vc4_clock_us()+1500000;
        while (!(input=h264dec_get_input_buffer(&capacity)) && (int32_t)(vc4_clock_us()-timeout)<0)
            h264dec_poll();
        if (!input || end-at>capacity) {
            if (input) h264dec_cancel_input();
            xdos_video_log("[H264] Cannot stage access unit %u",shown);
            break;
        }
        memcpy(input,file+at,end-at);
        xdos_video_cache_sync((uint32_t)(uintptr_t)input,end-at);
        video_ready_frame=0;
        if (!h264dec_submit_input(end-at,(int64_t)shown*66667,false)) break;
        while (!video_ready_frame && (int32_t)(vc4_clock_us()-timeout)<0)
            h264dec_poll();
        if (!video_ready_frame) {
            xdos_video_log("[H264] Decoder produced no frame for access unit %u",shown);
            break;
        }
        while ((int32_t)(vc4_clock_us()-(started+shown*66667))<0) vc4_poll();
        video_prepare(textures,video_ready_frame);
        if (!video_swap()) break;
        h264dec_recycle_output(video_ready_frame);
        if (++shown==1) xdos_video_log("[H264] First hardware-decoded frame presented");
        at=end;
    }
done:
    xdos_video_log("[H264] decoded=%u presented=%u",h264dec_frames_decoded(),shown);
    if (h264dec_shutdown())
        for (unsigned i=0;i<3;++i) vchiq_free_shared(handles[i]);
    free(file);
}
