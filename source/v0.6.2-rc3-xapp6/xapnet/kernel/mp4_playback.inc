bool h264dec_failed(void);   /* XDOS: h264dec.c, set on MMAL_EVENT_ERROR */
extern unsigned xdos_vc_mem_mb;   /* XDOS: kernel.cpp, GPU memory split in MB */
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

typedef struct VideoTiming {
    unsigned max, count;
    uint64_t sum;
    unsigned bins[256];
} VideoTiming;

static void record_timing(VideoTiming *timing,unsigned elapsed)
{
    const unsigned bin=elapsed/1000;
    ++timing->bins[bin<255 ? bin : 255];
    ++timing->count;
    timing->sum+=elapsed;
    if (elapsed>timing->max) timing->max=elapsed;
}

static unsigned timing_p95(const VideoTiming *timing)
{
    const unsigned rank=timing->count-timing->count/20;
    unsigned bin=0, seen=timing->bins[0];
    while (seen<rank && bin<255) seen+=timing->bins[++bin];
    return bin;
}

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
        VideoWidth>1920 || VideoHeight>1088 || (VideoWidth&31) ||
        ((VideoHeight&15) && VideoHeight!=1080))
        goto close_file;
    VideoBufferHeight=(VideoHeight+15)&~15u;
    VideoFrameBytes=VideoWidth*VideoBufferHeight*3/2;
    xdos_video_log("[PLAY] I420 buffer=%ux%u visible=%ux%u",
                   VideoWidth,VideoBufferHeight,VideoWidth,VideoHeight);
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
    if (!video_graphics(textures)) {
        xdos_video_log("[PLAY] Renderer setup failed");
        goto restore_context;
    }
    /* XDOS: with gpu_mem=64 and a 1080p desktop the decoder runs out of GPU memory
       above SD sizes (MMAL error 2) and the VideoCore stops answering: refuse first. */
    if (xdos_vc_mem_mb && xdos_vc_mem_mb < 128 && VideoWidth*VideoHeight > 640u*480u) {
        xdos_video_log("[PLAY] GPU memory %u MB is too small for %ux%u: set gpu_mem=128 in config.txt",
                       xdos_vc_mem_mb,VideoWidth,VideoHeight);
        goto restore_context;
    }
    if (!h264dec_init(VideoWidth,VideoHeight,queue_video_frame)) {
        xdos_video_log("[PLAY] Renderer or decoder setup failed");
        h264dec_shutdown();
        goto restore_context;
    }
    uint32_t handles[H264DEC_MAX_OUTPUT]={0};
    unsigned submitted=0, presented=0;
    VideoTiming intervals={0}, uploads={0}, swaps={0}, reads={0};
    unsigned last_swap=0, empty_since=0, empty_max=0, starved=0;
    unsigned pts_backwards=0, pts_max_gap=0;
    int64_t last_pts=0;
    bool was_empty=false, waited_past_deadline=false;
    const uint64_t nominal_us=((uint64_t)info->duration_hi*0x100000000ull+
                              info->duration_lo)*1000000/info->timescale/info->sample_count;
    int64_t next_pts=0;
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
        if (h264dec_failed()) {   // MMAL_EVENT_ERROR: stop feeding a dead component
            xdos_video_log("[PLAY] decoder reported an error; stopping");
            break;
        }
        const unsigned now=vc4_clock_us();
        if (presented && !video_count && !video_eos &&
            (int64_t)(unsigned)(now-start)>=next_pts-first_pts) {
            if (!was_empty) { was_empty=true; empty_since=now; }
            waited_past_deadline=true;
        }
        if (video_count) {
            const VideoFrame frame=video_frames[0];
            if (!presented) { first_pts=frame.pts; start=now; }
            if (was_empty) {
                const unsigned empty=now-empty_since;
                if (empty>empty_max) empty_max=empty;
                was_empty=false;
            }
            const int64_t elapsed=(unsigned)(now-start);
            const bool due=frame.pts-first_pts<=elapsed;
            if (!prepared && (XDOS_PLAYER_OPTIMIZED || due)) {
                const unsigned upload_start=vc4_clock_us();
                glClear(GL_COLOR_BUFFER_BIT);
                video_prepare(textures,frame.address);
                if (XDOS_PLAYER_OPTIMIZED) glFlush();
                record_timing(&uploads,vc4_clock_us()-upload_start);
                prepared=true;
            }
            if (due) {
                const unsigned swap_start=vc4_clock_us();
                if (!video_swap()) goto stop;
                const unsigned swap=vc4_clock_us();
                record_timing(&swaps,swap-swap_start);
                if (presented) record_timing(&intervals,swap-last_swap);
                if (presented) {
                    if (frame.pts<=last_pts) ++pts_backwards;
                    else if ((uint64_t)(frame.pts-last_pts)>pts_max_gap)
                        pts_max_gap=(unsigned)(frame.pts-last_pts);
                }
                last_pts=frame.pts;
                if (waited_past_deadline) ++starved;
                waited_past_deadline=false;
                next_pts=frame.pts+(int64_t)nominal_us;
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
                const unsigned read_start=vc4_clock_us();
                const unsigned bytes=read_access_unit(&mp4,track,submitted,buffer,capacity,
                                                       read_at,file,size,&pts,sample_buffer);
                record_timing(&reads,vc4_clock_us()-read_start);
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
        xdos_video_log("[PLAY TIMING] mode=%u samples=%u p95_bucket_ms=%u max_us=%u unpresented=%u",
                       XDOS_PLAYER_OPTIMIZED,intervals.count,timing_p95(&intervals),intervals.max,
                       presented<info->sample_count ? info->sample_count-presented : 0);
        xdos_video_log("[PLAY PIPE] queue_late_frames=%u queue_late_max_us=%u nominal_us=%u",
                       starved,empty_max,(unsigned)nominal_us);
        xdos_video_log("[PLAY PIPE] pts_nonincreasing=%u pts_max_gap_us=%u",pts_backwards,pts_max_gap);
        xdos_video_log("[PLAY PIPE] read_p95_ms=%u read_max_us=%u prep_p95_ms=%u prep_max_us=%u swap_p95_ms=%u swap_max_us=%u",
                       timing_p95(&reads),reads.max,timing_p95(&uploads),uploads.max,
                       timing_p95(&swaps),swaps.max);
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
