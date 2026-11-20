#include "xapp_keypad.h"

typedef struct {
    int32_t accumulator;
    int32_t entry;
    int operation;
    int new_entry;
} Calculator;

static void copy_text(char *target, const char *source)
{
    while ((*target++ = *source++)) {}
}

static void number(char *target, int32_t value)
{
    uint32_t magnitude = (uint32_t)value;
    if (value < 0) {
        *target++ = '-';
        magnitude = 0U - magnitude;
    }
    char digits[10];
    unsigned n = 0;
    do {
        digits[n++] = '0' + magnitude % 10U;
        magnitude /= 10U;
    } while (magnitude);
    while (n) *target++ = digits[--n];
    *target = 0;
}

static void input(void *state, int key, char *display)
{
    Calculator *calc = state;
    if (!key || key == 'C' || key == 'c') {
        calc->accumulator = calc->entry = calc->operation = calc->new_entry = 0;
    } else if (key >= '0' && key <= '9') {
        if (calc->new_entry) {
            calc->entry = 0;
            calc->new_entry = 0;
        }
        if (calc->entry <= (INT32_MAX - (key - '0')) / 10)
            calc->entry = calc->entry * 10 + key - '0';
    } else if (key == '=' || key == '\r' || key == '\n') {
        int64_t result = calc->accumulator;
        switch (calc->operation) {
        case '+': result += calc->entry; break;
        case '-': result -= calc->entry; break;
        case '*': result *= calc->entry; break;
        case '/':
            if (!calc->entry) {
                calc->operation = 0;
                calc->new_entry = 1;
                copy_text(display, "DIV BY ZERO");
                return;
            }
            if (calc->accumulator == INT32_MIN && calc->entry == -1)
                result = (int64_t)INT32_MAX + 1;
            else
                result = calc->accumulator / calc->entry;
            break;
        default: return;
        }
        calc->operation = 0;
        calc->new_entry = 1;
        if (result > INT32_MAX || result < INT32_MIN) {
            copy_text(display, "OVERFLOW");
            return;
        }
        calc->entry = (int32_t)result;
    } else if (key == '+' || key == '-' || key == '*' || key == '/') {
        calc->accumulator = calc->entry;
        calc->operation = key;
        calc->new_entry = 1;
    } else {
        return;
    }
    number(display, calc->entry);
}

int32_t xapp_entry(const XdosHostApiV1 *base)
{
    if (!base || base->abi_version != XDOS_ABI_VERSION ||
        base->struct_bytes < sizeof(XdosKeypadApi)) return -1;
    const XdosKeypadApi *api = (const XdosKeypadApi *)base;
    return api->open_keypad(base->context, "Calculator", "789/456*123-C0=+", input);
}
