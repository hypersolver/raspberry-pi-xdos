#include <lvgl.h>
#include "xdos_corejobs.h"
#include "xdos_sdk_v1.h"

namespace {
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
    auto *calc = static_cast<Calculator *>(state);
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

Calculator calculator;
lv_obj_t *readout;
int slot;
constexpr char Keys[]="789/456*123-C0=+";

void Key(void *,unsigned key)
{
    char display[32]{};
    if (key>127) return;
    input(&calculator,static_cast<int>(key),display);
    if (*display) lv_label_set_text(readout,display);
}

void Click(lv_event_t *event)
{
    Key(nullptr,static_cast<unsigned>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event))));
}

void Close(void *) { xdos_v2_exit(slot); }
}

extern "C" int32_t xapp_entry(const XdosHostApiV1 *api)
{
    if (!api || api->abi_version!=XDOS_ABI_VERSION) return -1;
    slot=xdos_v2_slot_of(reinterpret_cast<const void *>(xapp_entry));
    auto *body=slot>=0 ? xdos_v2_window(slot,"Calculator",320,305) : nullptr;
    if (!body) return -1;
    readout=lv_label_create(body);
    lv_label_set_text(readout,"0");
    lv_obj_align(readout,LV_ALIGN_TOP_RIGHT,-10,10);
    for (unsigned i=0;i<sizeof Keys-1;++i) {
        auto *button=lv_button_create(body);
        lv_obj_set_size(button,62,42);
        lv_obj_set_pos(button,8+72*(i%4),45+52*(i/4));
        auto *label=lv_label_create(button);
        const char text[]={Keys[i],0};
        lv_label_set_text(label,text);
        lv_obj_center(label);
        lv_obj_add_event_cb(button,Click,LV_EVENT_CLICKED,
                            reinterpret_cast<void *>(static_cast<uintptr_t>(Keys[i])));
    }
    xdos_v2_register_close(slot,Close,nullptr);
    xdos_v2_register_key(slot,Key,nullptr);
    return 0;
}
