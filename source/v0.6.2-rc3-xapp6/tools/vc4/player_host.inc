extern "C" int xdos_vc4_play_mp4(int (*read_at)(int64_t,void *,size_t,void *),
                               void *,int64_t,int (*cancelled)());
extern "C" void xdos_video_set_log(void (*sink)(const char *));
extern "C" unsigned vc4_clock_us() { return CTimer::GetClockTicks(); }
extern "C" void vc4_poll() { CScheduler::Get()->Yield(); }

namespace {
char playerPath[208];
CSerialDevice *playerSerial;
bool playerCancelled;

int ReadVideo(int64_t offset,void *data,size_t bytes,void *opaque)
{
    auto *file=static_cast<FIL *>(opaque);
    if (offset<0 || static_cast<uint64_t>(offset)>f_size(file) ||
        bytes>f_size(file)-static_cast<uint64_t>(offset)) return -1;
    if (f_tell(file)!=static_cast<FSIZE_t>(offset) &&
        f_lseek(file,static_cast<FSIZE_t>(offset))!=FR_OK) return -1;
    UINT count;
    return f_read(file,data,bytes,&count)==FR_OK && count==bytes ? 0 : -1;
}

bool QueueVideo(const char *path)
{
    if (!path || !*path || playerPath[0]) return false;
    const size_t length=strlen(path);
    if (length<5 || length>191 || strcasecmp(path+length-4,".mp4")) return false;
    const char *prefix="SD:/XDOS/INBOX/";
    if (!strncmp(path,"SD:/",4) || !strncmp(path,"sd:/",4)) prefix="";
    else if (*path=='/') prefix="SD:";
    for (const unsigned char *p=reinterpret_cast<const unsigned char *>(path);*p;++p) {
        if (*p<32 || *p==127 || *p=='\\' || (*p==':' && p!=reinterpret_cast<const unsigned char *>(path)+2))
            return false;
    }
    strcpy(playerPath,prefix);
    strcat(playerPath,path);
    FIL file;
    if (f_open(&file,playerPath,FA_READ)!=FR_OK) {
        playerPath[0]=0;
        return false;
    }
    f_close(&file);
    return true;
}

int StopVideo()
{
    if (playerSerial) {
        auto &uart=TerminalUart();
        uart.Poll(*playerSerial,true);
        const auto action=uart.TakeOpenRequest();
        if (action==XdosUartService::Shortcut::Close) {
            playerCancelled=true;
            uart.CompleteClose(*playerSerial,true);
        } else if (action!=XdosUartService::Shortcut::None) {
            uart.CompleteOpen(*playerSerial,false);
        }
    }
    return playerCancelled;
}

int RunQueuedVideo(CSerialDevice *serial)
{
    FIL file;
    int result=-1;
    if (f_open(&file,playerPath,FA_READ)==FR_OK) {
        playerCancelled=false;
        playerSerial=serial;
        xdos_video_set_log([](const char *message) {
            if (playerSerial) {
                playerSerial->Write(message,strlen(message));
                playerSerial->Write("\r\n",2);
            }
        });
        result=xdos_vc4_play_mp4(ReadVideo,&file,f_size(&file),StopVideo);
        xdos_video_set_log(nullptr);
        playerSerial=nullptr;
        f_close(&file);
        if (playerCancelled) result=0;
    }
    playerPath[0]=0;
    return result;
}
}
