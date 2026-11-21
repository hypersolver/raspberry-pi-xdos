#include <Arduino.h>

// Inspect this counter with a debugger while the sketch is open.
volatile uint32_t iterations;

void setup()
{
    iterations = 0;
}

void loop()
{
    iterations = iterations + 1;
}
