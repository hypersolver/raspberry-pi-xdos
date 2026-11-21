#include "Arduino.h"
#include <circle/actled.h>
#include <circle/gpiopin.h>
#include <circle/startup.h>
#include <circle/timer.h>

namespace {
constexpr unsigned gpioCount = 28;
CGPIOPin pins[gpioCount];
bool assigned[gpioCount] = {};
bool activityLedOn = false;
uint64_t startTicks;

uint64_t elapsedMicroseconds()
{
    return CTimer::GetClockTicks64() - startTicks;
}
}

void pinMode(uint8_t pin, uint8_t mode)
{
    if (pin >= gpioCount)
        return;

    TGPIOMode gpioMode;
    switch (mode) {
    case INPUT: gpioMode = GPIOModeInput; break;
    case OUTPUT: gpioMode = GPIOModeOutput; break;
    case INPUT_PULLUP: gpioMode = GPIOModeInputPullUp; break;
    case INPUT_PULLDOWN: gpioMode = GPIOModeInputPullDown; break;
    default: return;
    }

    if (!assigned[pin]) {
        pins[pin].AssignPin(pin);
        assigned[pin] = true;
    }
    pins[pin].SetMode(gpioMode);
}

void digitalWrite(uint8_t pin, uint8_t value)
{
    if (pin == LED_BUILTIN) {
        activityLedOn = value != LOW;
        if (activityLedOn)
            CActLED::Get()->On();
        else
            CActLED::Get()->Off();
    } else if (pin < gpioCount && assigned[pin]) {
        pins[pin].Write(value != LOW);
    }
}

int digitalRead(uint8_t pin)
{
    if (pin == LED_BUILTIN)
        return activityLedOn;

    if (pin < gpioCount && assigned[pin])
        return pins[pin].Read() ? HIGH : LOW;

    return LOW;
}

unsigned long millis(void)
{
    return elapsedMicroseconds() / 1000;
}

unsigned long micros(void)
{
    return elapsedMicroseconds();
}

void delay(unsigned long milliseconds)
{
    while (milliseconds > 1000) {
        CTimer::SimpleMsDelay(1000);
        milliseconds -= 1000;
    }
    CTimer::SimpleMsDelay(milliseconds);
}

void delayMicroseconds(unsigned int microseconds)
{
    CTimer::SimpleusDelay(microseconds);
}

void yield(void)
{
}

int main(void)
{
    CActLED activityLed;
    startTicks = CTimer::GetClockTicks64();
    setup();
    for (;;)
        loop();
}
