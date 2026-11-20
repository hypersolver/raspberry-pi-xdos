static uint16_t scroll_pixels[1024 * 768];

static void scroll_log(const char *label, unsigned count, uint64_t upload,
                       uint64_t compose, uint64_t swap, uint64_t frame,
                       unsigned max_frame)
{
   printk("[VC4] %s frames=%u upload_mean_us=%u compose_mean_us=%u swap_mean_us=%u "
          "frame_mean_us=%u frame_max_us=%u\n", label, count, (unsigned)(upload/count),
          (unsigned)(compose/count), (unsigned)(swap/count),
          (unsigned)(frame/count), max_frame);
}

static GLuint scroll_shader(GLenum type, const char *source)
{
   GLuint id=glCreateShader(type);
   glShaderSource(id,1,&source,NULL);
   glCompileShader(id);
   GLint ok;
   glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
   if (!ok) showlog(id);
   assert(ok);
   return id;
}

static void scroll_offscreen(GLuint source, GLint offset)
{
   GLuint target, framebuffer;
   glGenTextures(1,&target);
   glBindTexture(GL_TEXTURE_2D,target);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
   glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1024,768,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
   glGenFramebuffers(1,&framebuffer);
   glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
   glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);
   if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) {
      printk("[VC4] offscreen framebuffer unavailable\n");
   } else {
      glBindTexture(GL_TEXTURE_2D,source);
      glDrawArrays(GL_TRIANGLE_STRIP,0,4);
      glFinish();
      for (unsigned phase=0;phase<2;++phase) {
         const unsigned count=120;
         uint64_t upload=0,compose=0;
         for (unsigned i=0;i<count;++i) {
            unsigned start=vc4_clock_us();
            if (phase) {
               glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1024,768,GL_RGB,
                               GL_UNSIGNED_SHORT_5_6_5,scroll_pixels);
               glFinish();
            }
            const unsigned uploaded=vc4_clock_us();
            if (phase) upload+=uploaded-start;
            glUniform1f(offset,(GLfloat)(i%256)/256.0f);
            glDrawArrays(GL_TRIANGLE_STRIP,0,4);
            glFinish();
            compose+=vc4_clock_us()-uploaded;
            check();
            vc4_poll();
         }
         printk("[VC4] offscreen_%s frames=%u upload_mean_us=%u compose_mean_us=%u\n",
                phase ? "streamed" : "cached",count,(unsigned)(upload/count),
                (unsigned)(compose/count));
      }
   }
   glBindFramebuffer(GL_FRAMEBUFFER,0);
   glDeleteFramebuffers(1,&framebuffer);
   glDeleteTextures(1,&target);
   glBindTexture(GL_TEXTURE_2D,source);
   glFinish();
}

static void texture_scroll(void)
{
   const char *vertex =
       "attribute vec2 pos; varying vec2 uv;"
       "void main(){gl_Position=vec4(pos,0.,1.);uv=(pos+1.)*.5;}";
   const char *fragment =
       "precision mediump float; varying vec2 uv; uniform sampler2D img;"
       "uniform float offset; void main(){"
       "gl_FragColor=texture2D(img,vec2(fract(uv.x-offset),uv.y));}";
   GLuint program=glCreateProgram();
   glAttachShader(program,scroll_shader(GL_VERTEX_SHADER,vertex));
   glAttachShader(program,scroll_shader(GL_FRAGMENT_SHADER,fragment));
   glLinkProgram(program);
   GLint linked;
   glGetProgramiv(program,GL_LINK_STATUS,&linked);
   if (!linked) showprogramlog(program);
   assert(linked);
   glUseProgram(program);

   for (unsigned y=0;y<768;++y)
      for (unsigned x=0;x<1024;++x) {
         unsigned color=((x/64+y/48)&1) ? 0x1d9e : 0xe885;
         if (x%128<5 || y%96<4) color=0xffff;
         scroll_pixels[y*1024+x]=color;
      }

   GLuint texture;
   glGenTextures(1,&texture);
   glBindTexture(GL_TEXTURE_2D,texture);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
   glPixelStorei(GL_UNPACK_ALIGNMENT,2);
   unsigned start=vc4_clock_us();
   glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,1024,768,0,GL_RGB,
                GL_UNSIGNED_SHORT_5_6_5,scroll_pixels);
   glFinish();
   unsigned initial_upload=vc4_clock_us()-start;
   check();

   EGLint width=0,height=0;
   assert(eglQuerySurface(state->display,state->surface,EGL_WIDTH,&width));
   assert(eglQuerySurface(state->display,state->surface,EGL_HEIGHT,&height));
   assert(width==1024 && height==768);
   glViewport(0,0,width,height);
   glUniform1i(glGetUniformLocation(program,"img"),0);
   const GLint offset=glGetUniformLocation(program,"offset");
   const GLint pos=glGetAttribLocation(program,"pos");
   const GLfloat vertices[]={-1,-1, 1,-1, -1,1, 1,1};
   glVertexAttribPointer(pos,2,GL_FLOAT,GL_FALSE,0,vertices);
   glEnableVertexAttribArray(pos);
   glUniform1f(offset,0);
   assert(eglSwapInterval(state->display,1));
   printk("[VC4] texture=1024x768 rgb565 bytes=1572864 initial_upload_us=%u swap_interval=1\n",initial_upload);
   scroll_offscreen(texture,offset);
   for (unsigned phase=0;phase<3;++phase) {
      const unsigned count=180;
      uint64_t upload=0,compose=0,swap=0,frame=0;
      unsigned max_frame=0;
      for (unsigned i=0; i<count || phase==2; ++i) {
         const unsigned begin=vc4_clock_us();
         unsigned t=begin;
         if (phase==1) {
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1024,768,GL_RGB,
                            GL_UNSIGNED_SHORT_5_6_5,scroll_pixels);
            glFinish();
            t=vc4_clock_us();
            upload+=t-begin;
         }
         glUniform1f(offset,(GLfloat)(i%256)/256.0f);
         glDrawArrays(GL_TRIANGLE_STRIP,0,4);
         glFinish();
         const unsigned drawn=vc4_clock_us();
         assert(eglSwapBuffers(state->display,state->surface));
         const unsigned shown=vc4_clock_us();
         compose+=drawn-t;
         swap+=shown-drawn;
         frame+=shown-begin;
         if (shown-begin>max_frame) max_frame=shown-begin;
         check();
         vc4_poll();
      }
      scroll_log(phase==0 ? "cached" : "streamed",count,upload,compose,swap,frame,max_frame);
   }
}
