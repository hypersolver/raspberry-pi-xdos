#include "xapp_arduino_lvgl.h"

void setup();
void loop();
#ifdef XDOS_BENCHMARK_SKETCH
extern "C" void xdos_stop_official_benchmark();
#endif
#ifdef XDOS_LED_BLINK_SKETCH
extern "C" void xdos_stop_blink();
#endif
#ifdef XDOS_IMAGE_VIEW_SKETCH
extern "C" void xdos_stop_image_view();
#endif
#ifdef XDOS_MEDIA_SKETCH
extern "C" void xdos_media_stop_music();
#endif

extern "C" int32_t xapp_entry(const XdosHostApiV1 *base)
{
    if (!base || base->abi_version != XDOS_ABI_VERSION ||
        base->struct_bytes < sizeof(XdosArduinoLvglApi)) return -1;
    const auto *api = reinterpret_cast<const XdosArduinoLvglApi *>(base);
    if (!api->run_sketch) return -1;
#ifdef XDOS_BENCHMARK_SKETCH
    return api->run_sketch(base->context,setup,loop,xdos_stop_official_benchmark,1);
#elif defined(XDOS_LED_BLINK_SKETCH)
    return api->run_sketch(base->context,setup,loop,xdos_stop_blink,0);
#elif defined(XDOS_IMAGE_VIEW_SKETCH)
    return api->run_sketch(base->context,setup,loop,xdos_stop_image_view,0);
#elif defined(XDOS_MEDIA_SKETCH)
    return api->run_sketch(base->context,setup,loop,xdos_media_stop_music,0);
#else
    return api->run_sketch(base->context,setup,loop,nullptr,0);
#endif
}
