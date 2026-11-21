#include "video_transport.h"
#include <circle/bcmpropertytags.h>
#include <circle/synchronize.h>
#include <circle/logger.h>
#include <circle/string.h>
#include <stdarg.h>
#include <vc4/vchi/vchi.h>

namespace {
VCHI_INSTANCE_T instance;
bool connected;
void (*logSink)(const char *);
struct Service {
    VCHI_SERVICE_HANDLE_T handle;
    vchiq_callbacks_t callbacks;
};
Service services[2]{};

void ServiceEvent(void *, VCHI_CALLBACK_REASON_T, void *) {}

bool MemoryTag(unsigned tag,unsigned handle,unsigned &value)
{
    CBcmPropertyTags mailbox;
    TPropertyTagSimple request{};
    request.nValue=handle;
    if (!mailbox.GetTag(tag,&request,sizeof request,sizeof request.nValue)) return false;
    value=request.nValue;
    return true;
}
}

extern "C" bool vchiq_init()
{
    if (!connected)
        connected=vchi_initialise(&instance)==0 && vchi_connect(nullptr,0,instance)==0;
    return connected;
}

extern "C" int vchiq_open_service(uint32_t fourcc,short version,short minimum,
                                   const vchiq_callbacks_t *callbacks)
{
    for (unsigned i=0;i<sizeof services/sizeof services[0];++i) {
        auto &service=services[i];
        if (service.handle) continue;
        SERVICE_CREATION_T params{};
        params.version={static_cast<uint32_t>(version),static_cast<uint32_t>(minimum)};
        params.service_id=fourcc;
        params.callback=ServiceEvent;
        if (vchi_service_open(instance,&params,&service.handle)!=0) {
            service.handle=nullptr;
            return -1;
        }
        service.callbacks=*callbacks;
        return i;
    }
    return -1;
}

extern "C" bool vchiq_close_service(int id)
{
    auto &service=services[id];
    if (vchi_service_close(service.handle)!=0) return false;
    service={};
    return true;
}

extern "C" bool vchiq_queue_message(int id,const void *data,unsigned bytes)
{
    return vchi_msg_queue(services[id].handle,data,bytes,VCHI_FLAGS_NONE,nullptr)==0;
}

extern "C" void vchiq_poll()
{
    vc4_poll();
    alignas(8) unsigned char message[4096];
    for (auto &service : services) {
        uint32_t bytes;
        while (service.handle &&
               vchi_msg_dequeue(service.handle,message,sizeof message,&bytes,VCHI_FLAGS_NONE)==0 && bytes)
            if (service.callbacks.on_data) service.callbacks.on_data(message,bytes);
    }
}

extern "C" uint32_t vchiq_alloc_shared(uint32_t size,uint32_t *handle)
{
    struct Allocation {
        TPropertyTag tag;
        uint32_t size, alignment, flags;
    } request{};
    request.size=size;
    request.alignment=4096;
    request.flags=(1u<<2)|(1u<<4); // Direct alias, zero initialized.
    CBcmPropertyTags mailbox;
    *handle=0;
    if (!mailbox.GetTag(0x3000c,&request,sizeof request,12) || !request.size) {
        xdos_video_log("[PLAY] VC mailbox could not allocate %u bytes",size);
        return 0;
    }
    *handle=request.size;
    unsigned busAddress=0;
    if (MemoryTag(0x3000d,*handle,busAddress) && busAddress) return busAddress&0x3fffffffu;
    xdos_video_log("[PLAY] VC mailbox could not lock handle %u",*handle);
    vchiq_free_shared(*handle);
    *handle=0;
    return 0;
}

extern "C" void vchiq_free_shared(uint32_t handle)
{
    if (!handle) return;
    unsigned result;
    MemoryTag(0x3000e,handle,result);
    MemoryTag(0x3000f,handle,result);
}

extern "C" void xdos_video_cache_sync(uint32_t address,uint32_t bytes)
{
    CleanAndInvalidateDataCacheRange(address,bytes);
}

extern "C" void xdos_video_log(const char *format,...)
{
    CString message;
    va_list args;
    va_start(args,format);
    message.FormatV(format,args);
    va_end(args);
    if (logSink) logSink(message);
    else CLogger::Get()->Write("VIDEO",LogNotice,"%s",(const char *)message);
}

extern "C" void xdos_video_set_log(void (*sink)(const char *))
{
    logSink=sink;
}
