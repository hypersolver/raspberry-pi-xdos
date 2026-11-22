#include <stdint.h>

void setup();
void loop();
extern "C" void xdos_sketch_stop() __attribute__((weak));

namespace {
using Routine = void (*)();

struct SketchApi {
    uint32_t version;
    uint32_t bytes;
    void *context;
    void (*log)(void *, const char *);
    int32_t (*showDemo)(void *);
    int32_t (*showBenchmark)(void *);
    int32_t (*runSketch)(void *, Routine, Routine, Routine, int);
};
}

extern "C" int32_t xapp_entry(const SketchApi *api)
{
    if (!api || api->version != 1 || api->bytes < sizeof(SketchApi) || !api->runSketch)
        return -1;
    return api->runSketch(api->context,setup,loop,xdos_sketch_stop,0);
}
