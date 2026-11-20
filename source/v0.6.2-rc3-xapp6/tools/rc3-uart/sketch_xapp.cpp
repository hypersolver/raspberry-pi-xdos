#include "xapp_arduino_lvgl.h"

void setup();
void loop();
#ifdef XDOS_BENCHMARK_SKETCH
extern "C" void xdos_stop_official_benchmark();
#endif

extern "C" int32_t xapp_entry(const XdosHostApiV1 *base)
{
    if (!base || base->abi_version != XDOS_ABI_VERSION ||
        base->struct_bytes < sizeof(XdosArduinoLvglApi)) return -1;
    const auto *api = reinterpret_cast<const XdosArduinoLvglApi *>(base);
    if (!api->run_sketch) return -1;
#ifdef XDOS_BENCHMARK_SKETCH
    return api->run_sketch(base->context,setup,loop,xdos_stop_official_benchmark,1);
#else
    return api->run_sketch(base->context,setup,loop,nullptr,0);
#endif
}
