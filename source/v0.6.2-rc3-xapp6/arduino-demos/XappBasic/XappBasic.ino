#include <Arduino.h>  // Arduino core types and timing functions. / Arduino 核心型別與計時函式。

// Inspect this counter with a debugger while the sketch is open. / 草稿執行時可用除錯器查看此計數。
volatile uint32_t iterations;  // Keep the counter visible to a debugger. / 讓除錯器可查看計數。

void setup()  // Run once when the sketch starts. / 草稿啟動時執行一次。
{
    iterations = 0;  // Reset the counter on startup. / 啟動時將計數歸零。
}

void loop()  // Run repeatedly while the sketch is active. / 草稿執行期間重複呼叫。
{
    iterations = iterations + 1;  // Record one pass through the sketch loop. / 記錄主迴圈執行一次。
}
