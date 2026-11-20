#include <lvgl/lvgl.h>
#include <lvgl/src/misc/lv_timer_private.h>

namespace {
struct BenchmarkRow {
    char name[80];
    unsigned samples, fps, renderMs, flushMs;
};

BenchmarkRow benchmarkRows[40]{};
unsigned benchmarkCount;
bool benchmarkRunning, benchmarkComplete;
int benchmarkVc4Start, benchmarkVc4End;
unsigned benchmarkRefreshStart, benchmarkRefreshEnd;

unsigned BenchmarkRefreshPeriod()
{
    const auto *timer = lv_display_get_refr_timer(nullptr);
    return timer ? timer->period : 0;
}
}

extern "C" int xdos_vc4_active(void) __attribute__((weak));

extern "C" void xdos_benchmark_begin(void)
{
    benchmarkCount = 0;
    benchmarkComplete = false;
    benchmarkRunning = true;
    benchmarkVc4Start = xdos_vc4_active ? xdos_vc4_active() : 0;
    benchmarkRefreshStart = BenchmarkRefreshPeriod();
}

extern "C" void xdos_benchmark_row(const char *name, unsigned samples, unsigned fps,
                                    unsigned renderMs, unsigned flushMs)
{
    if (benchmarkCount >= sizeof benchmarkRows / sizeof benchmarkRows[0]) return;
    auto &row = benchmarkRows[benchmarkCount++];
    strncpy(row.name, name, sizeof row.name - 1);
    row.name[sizeof row.name - 1] = 0;
    row.samples = samples;
    row.fps = fps;
    row.renderMs = renderMs;
    row.flushMs = flushMs;
}

extern "C" void xdos_benchmark_end(int complete)
{
    if (!benchmarkRunning) return;
    benchmarkVc4End = xdos_vc4_active ? xdos_vc4_active() : 0;
    benchmarkRefreshEnd = BenchmarkRefreshPeriod();
    benchmarkComplete = complete != 0;
    benchmarkRunning = false;
}

void XdosPrintBenchmarkReport(CSerialDevice &serial)
{
    CString line;
    if (benchmarkComplete) {
        line.Format("@XDOS BENCH META,%u,%d,%d,%u,%u\r\n", benchmarkCount,
                    benchmarkVc4Start, benchmarkVc4End, benchmarkRefreshStart, benchmarkRefreshEnd);
        serial.Write(line, line.GetLength());
        for (unsigned i = 0; i < benchmarkCount; ++i) {
            const auto &row = benchmarkRows[i];
            line.Format("@XDOS BENCH ROW,%s,%u,%u,%u,%u\r\n", row.name, row.samples,
                        row.fps, row.renderMs, row.flushMs);
            serial.Write(line, line.GetLength());
        }
    }
    line.Format("@XDOS BENCH %s @XDOS OK\r\n",
                benchmarkComplete ? "COMPLETE" : benchmarkRunning ? "RUNNING" : "UNAVAILABLE");
    serial.Write(line, line.GetLength());
}
