#include "video_transport.h"
#include "h264dec.h"

// Circle's lightweight C library omits these two minimp4 muxer helpers.
static void *mp4_memchr(const void *data,int value,size_t bytes)
{
    const unsigned char *p=data;
    while (bytes--) {
        if (*p==(unsigned char)value) return (void *)p;
        ++p;
    }
    return NULL;
}

static char *mp4_strdup(const char *text)
{
    const size_t bytes=strlen(text)+1;
    char *copy=malloc(bytes);
    return copy ? memcpy(copy,text,bytes) : NULL;
}

#define memchr mp4_memchr
#define strdup mp4_strdup
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"
#undef memchr
#undef strdup

typedef struct VideoFrame {
    uint32_t address;
    int64_t pts;
} VideoFrame;

static VideoFrame video_frames[H264DEC_MAX_OUTPUT];
static unsigned video_count;
static bool video_eos;

static void queue_video_frame(uint32_t address, int64_t pts, bool eos)
{
    if (eos) video_eos=true;
    if (address && video_count<H264DEC_MAX_OUTPUT)
        video_frames[video_count++]=(VideoFrame){address,pts};
}

static bool append_nal(uint8_t *out, unsigned limit, unsigned *used,
                       const void *nal, unsigned bytes)
{
    if (!bytes || limit-*used<4 || bytes>limit-*used-4) return false;
    memcpy(out+*used,"\0\0\0\1",4);
    memcpy(out+*used+4,nal,bytes);
    *used+=4+bytes;
    return true;
}

static unsigned read_access_unit(const MP4D_demux_t *mp4, unsigned track,
                                 unsigned sample, uint8_t *out, unsigned limit,
                                 int (*read_at)(int64_t,void *,size_t,void *),
                                 void *file, int64_t file_size, unsigned *pts,
                                 uint8_t *sample_buffer)
{
    unsigned used=0, bytes=0, dts, duration;
    int sync;
    const int64_t offset=MP4D_frame_offset(mp4,track,sample,&bytes,&dts,pts,&duration,&sync);
    if (offset<0 || offset>file_size || bytes>file_size-offset || !bytes) return 0;
    if (sync || sample==0) {
        for (int pps=0;pps<2;++pps) {
            for (int i=0;;++i) {
                int count;
                const void *nal=pps ? MP4D_read_pps(mp4,track,i,&count)
                                    : MP4D_read_sps(mp4,track,i,&count);
                if (!nal) break;
                if (count<=0 || !append_nal(out,limit,&used,nal,count)) return 0;
            }
        }
    }
    const unsigned prefix=1+mp4->track[track].CodecDescription.avc1.lengthSizeMinusOne;
    int64_t cursor=offset;
    if (sample_buffer && (bytes>H264DEC_INPUT_BUF_SIZE ||
                          read_at(offset,sample_buffer,bytes,file))) return 0;
    while (bytes) {
        uint8_t length[4];
        if (bytes<prefix) return 0;
        if (sample_buffer) memcpy(length,sample_buffer+(cursor-offset),prefix);
        else if (read_at(cursor,length,prefix,file)) return 0;
        cursor+=prefix;
        bytes-=prefix;
        unsigned nal=0;
        for (unsigned i=0;i<prefix;++i) nal=(nal<<8)|length[i];
        if (!nal || nal>bytes || limit-used<4 || nal>limit-used-4) return 0;
        memcpy(out+used,"\0\0\0\1",4);
        if (sample_buffer) memcpy(out+used+4,sample_buffer+(cursor-offset),nal);
        else if (read_at(cursor,out+used+4,nal,file)) return 0;
        cursor+=nal;
        bytes-=nal;
        used+=4+nal;
    }
    return used;
}

int xdos_vc4_play_mp4(int (*read_at)(int64_t,void *,size_t,void *),
                      void *file, int64_t size, int (*cancelled)(void))
{
    if (!active) { xdos_video_log("[PLAY] VC4 desktop inactive"); return -1; }
    MP4D_demux_t mp4={0};
    if (!MP4D_open(&mp4,read_at,file,size)) {
        xdos_video_log("[PLAY] Cannot parse MP4 (%u bytes)",(unsigned)size);
        return -1;
    }
    int result=-1;
    unsigned track=0;
    while (track<mp4.track_count &&
           mp4.track[track].object_type_indication!=MP4_OBJECT_TYPE_AVC) ++track;
    if (track==mp4.track_count) {
        xdos_video_log("[PLAY] No AVC track in %u tracks",mp4.track_count);
        goto close_file;
    }
    const MP4D_track_t *info=&mp4.track[track];
    VideoWidth=info->SampleDescription.video.width;
    VideoHeight=info->SampleDescription.video.height;
    xdos_video_log("[PLAY] AVC %ux%u samples=%u timescale=%u",
                   VideoWidth,VideoHeight,info->sample_count,info->timescale);
    if (!info->timescale || !info->sample_count || !VideoWidth || !VideoHeight ||
        VideoWidth>1920 || VideoHeight>1088 || (VideoWidth&31) || (VideoHeight&15))
        goto close_file;
    VideoFrameBytes=VideoWidth*VideoHeight*3/2;
    const bool resume_scroll=scroll_supported;
    if (spare_texture) {
        glDeleteFramebuffers(1,&scroll_fbo);
        glDeleteTextures(1,&spare_texture);
        glDeleteProgram(scroll_program);
        spare_texture=scroll_fbo=scroll_program=0;
        scroll_supported=0;
        glFinish();
    }
    const EGLint attributes[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    EGLContext player=eglCreateContext(display,config,EGL_NO_CONTEXT,attributes);
    if (player==EGL_NO_CONTEXT) {
        xdos_video_log("[PLAY] eglCreateContext failed: %x",eglGetError());
        goto restore_scroll;
    }
    GLuint textures[3]={0};
    if (!eglMakeCurrent(display,surface,surface,player)) {
        xdos_video_log("[PLAY] eglMakeCurrent failed: %x",eglGetError());
        goto close_context;
    }
    unsigned width=surface_width, height=surface_height;
    if (width*VideoHeight>height*VideoWidth) width=height*VideoWidth/VideoHeight;
    else height=width*VideoHeight/VideoWidth;
    glViewport((surface_width-width)/2,(surface_height-height)/2,width,height);
    glClearColor(0,0,0,1);
    video_count=0;
    video_eos=false;
    if (!video_graphics(textures) || !h264dec_init(VideoWidth,VideoHeight,queue_video_frame)) {
        xdos_video_log("[PLAY] Renderer or decoder setup failed");
        goto restore_context;
    }
    uint32_t handles[H264DEC_MAX_OUTPUT]={0};
    unsigned submitted=0, presented=0;
    unsigned intervals[256]={0}, max_interval=0, last_swap=0;
    bool prepared=false;
    uint8_t *sample_buffer=XDOS_PLAYER_OPTIMIZED ? malloc(H264DEC_INPUT_BUF_SIZE) : NULL;
    unsigned last_progress=vc4_clock_us(), start=0;
    int64_t first_pts=0;
    if (XDOS_PLAYER_OPTIMIZED && !sample_buffer) goto stop;
    for (unsigned i=0;i<H264DEC_MAX_OUTPUT;++i) {
        const uint32_t address=vchiq_alloc_shared(VideoFrameBytes,&handles[i]);
        if (!address || !h264dec_add_output_buffer(address,VideoFrameBytes)) goto stop;
    }
    while (!cancelled()) {
        h264dec_poll();
        const unsigned now=vc4_clock_us();
        if (video_count) {
            const VideoFrame frame=video_frames[0];
            if (!presented) { first_pts=frame.pts; start=now; }
            const int64_t elapsed=(unsigned)(now-start);
            const bool due=frame.pts-first_pts<=elapsed;
            if (!prepared && (XDOS_PLAYER_OPTIMIZED || due)) {
                glClear(GL_COLOR_BUFFER_BIT);
                video_prepare(textures,frame.address);
                if (XDOS_PLAYER_OPTIMIZED) glFlush();
                prepared=true;
            }
            if (due) {
                if (!video_swap()) goto stop;
                const unsigned swap=vc4_clock_us();
                if (presented) {
                    const unsigned interval=swap-last_swap;
                    unsigned bucket=interval/1000;
                    ++intervals[bucket<255 ? bucket : 255];
                    if (interval>max_interval) max_interval=interval;
                }
                last_swap=swap;
                prepared=false;
                --video_count;
                memmove(video_frames,video_frames+1,video_count*sizeof *video_frames);
                h264dec_recycle_output(frame.address);
                ++presented;
                last_progress=now;
            }
        }
        if (video_eos && !video_count) { result=presented ? 0 : -1; break; }
        if (submitted<info->sample_count && video_count+1<H264DEC_MAX_OUTPUT) {
            uint32_t capacity;
            uint8_t *buffer=h264dec_get_input_buffer(&capacity);
            if (buffer) {
                unsigned pts;
                const unsigned bytes=read_access_unit(&mp4,track,submitted,buffer,capacity,
                                                       read_at,file,size,&pts,sample_buffer);
                if (!bytes) { h264dec_cancel_input(); goto stop; }
                xdos_video_cache_sync((uint32_t)(uintptr_t)buffer,bytes);
                const bool last=++submitted==info->sample_count;
                if (!h264dec_submit_input(bytes,(int64_t)pts*1000000/info->timescale,last)) goto stop;
                last_progress=now;
            }
        }
        if ((unsigned)(now-last_progress)>3000000) break;
    }
stop:
    free(sample_buffer);
    if (presented>1) {
        const unsigned count=presented-1;
        const unsigned rank=count-count/20;
        unsigned p95=0, total=intervals[0];
        while (total<rank && p95<255) total+=intervals[++p95];
        xdos_video_log("[PLAY TIMING] mode=%u samples=%u p95_bucket_ms=%u max_us=%u unpresented=%u",
                       XDOS_PLAYER_OPTIMIZED,count,p95,max_interval,info->sample_count-presented);
    }
    xdos_video_log("[PLAY] submitted=%u decoded=%u presented=%u result=%d",
                   submitted,h264dec_frames_decoded(),presented,result);
    if (h264dec_shutdown())
        for (unsigned i=0;i<H264DEC_MAX_OUTPUT;++i) vchiq_free_shared(handles[i]);
restore_context:
    if (eglMakeCurrent(display,surface,surface,context)) {
        glDrawArrays(GL_TRIANGLE_STRIP,0,4);
        eglSwapBuffers(display,surface);
    }
close_context:
    eglDestroyContext(display,player);
restore_scroll:
    if (resume_scroll) setup_scroll(surface_width,surface_height);
close_file:
    MP4D_close(&mp4);
    return result;
}
