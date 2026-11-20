namespace {
constexpr int WindowScaleNormal = 256;
constexpr int WindowScaleSmall = 230;

void SetWindowScale(void *object, int32_t scale)
{
    auto *window = static_cast<lv_obj_t *>(object);
    lv_obj_set_style_transform_scale_x(window, scale, 0);
    lv_obj_set_style_transform_scale_y(window, scale, 0);
}

void SetClosingWindowScale(void *object, int32_t scale)
{
    SetWindowScale(object, scale);
    lv_obj_set_style_opa_layered(static_cast<lv_obj_t *>(object),
        (scale - WindowScaleSmall) * LV_OPA_COVER / (WindowScaleNormal - WindowScaleSmall), 0);
}

lv_anim_t *StartWindowAnimation(lv_obj_t *window, bool closing, void *owner,
                                lv_anim_completed_cb_t completed)
{
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, window);
    lv_anim_set_user_data(&animation, owner);
    lv_anim_set_values(&animation,
        closing ? lv_obj_get_style_transform_scale_x(window, LV_PART_MAIN) : WindowScaleSmall,
        closing ? WindowScaleSmall : WindowScaleNormal);
    lv_anim_set_duration(&animation, closing ? 140 : 160);
    lv_anim_set_exec_cb(&animation, closing ? SetClosingWindowScale : SetWindowScale);
    lv_anim_set_path_cb(&animation, closing ? lv_anim_path_ease_in : lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&animation, completed);
    return lv_anim_start(&animation);
}

void StyleWindowSurface(lv_obj_t *obj, unsigned background, unsigned foreground,
                        unsigned border, int borderWidth)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(background), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(foreground), 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(obj, borderWidth, 0);
    lv_obj_set_style_radius(obj, 0, 0);
}

lv_obj_t *MakeWindowButton(lv_obj_t *parent, const char *label,
                           lv_event_cb_t event, void *owner)
{
    auto *button = MakeButton(parent, label, event, owner);
    StyleWindowSurface(button, 0x294A6A, 0xFFFFFF, 0xB6C4D3, 1);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x4A7491), LV_STATE_PRESSED);
    return button;
}

lv_obj_t *MakeWindowTextArea(lv_obj_t *parent)
{
    auto *area = lv_textarea_create(parent);
    StyleWindowSurface(area, 0xFFFFFF, 0x203047, 0xB6C4D3, 1);
    return area;
}

void DragWindow(lv_event_t *event)
{
    auto *input = lv_indev_active();
    if (!input) return;
    auto *window = static_cast<lv_obj_t *>(lv_event_get_user_data(event));
    lv_point_t movement;
    lv_indev_get_vect(input, &movement);
    const int maxX = lv_obj_get_width(lv_obj_get_parent(window)) - lv_obj_get_width(window);
    const int maxY = lv_obj_get_height(lv_obj_get_parent(window)) - lv_obj_get_height(window);
    const int x = lv_obj_get_x(window) + movement.x;
    const int y = lv_obj_get_y(window) + movement.y;
    lv_obj_set_pos(window, LV_CLAMP(0, x, maxX), LV_CLAMP(0, y, maxY));
}
}

lv_obj_t *XDOSLVGL::OpenAppWindow(const char *title, int width, int height)
{
    const int desktopWidth = desktop_->Width();
    const int desktopHeight = desktop_->Height();
    demoScreen_ = lv_obj_create(homeScreen_);
    if (!demoScreen_) return nullptr;
    auto *window = demoScreen_;
    lv_obj_remove_flag(window, LV_OBJ_FLAG_SCROLLABLE);
    StyleWindowSurface(window, 0xF8FAFD, 0x203047, 0xB6C4D3, 1);
    lv_obj_set_style_pad_all(window, 0, 0);
    lv_obj_set_style_shadow_color(window, lv_color_hex(0x071522), 0);
    lv_obj_set_style_shadow_opa(window, LV_OPA_70, 0);
    lv_obj_set_style_shadow_width(window, 8, 0);
    lv_obj_set_style_shadow_offset_y(window, 5, 0);
    // Position from the top left so drag offsets use the same coordinate space.
    lv_obj_set_size(window, width, height);
    lv_obj_set_pos(window, (desktopWidth-width)/2, (desktopHeight-height)/2);
    lv_obj_set_style_transform_pivot_x(window, width/2, 0);
    lv_obj_set_style_transform_pivot_y(window, height/2, 0);

    auto *bar = lv_obj_create(window);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    StyleWindowSurface(bar, 0x294A6A, 0xFFFFFF, 0x4FC7BB, 2);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_size(bar, lv_pct(100), 35);
    lv_obj_add_event_cb(bar, DragWindow, LV_EVENT_PRESSING, window);

    auto *heading = lv_label_create(bar);
    lv_label_set_text(heading, title);
    lv_obj_align(heading, LV_ALIGN_LEFT_MID, 13, 0);
    demoBack_ = MakeWindowButton(bar, "X", DemoBack, this);
    StyleWindowSurface(demoBack_, 0xF27771, 0xFFFFFF, 0xF27771, 0);
    lv_obj_set_style_bg_color(demoBack_, lv_color_hex(0xCC5854), LV_STATE_PRESSED);
    lv_obj_set_style_pad_all(demoBack_, 0, 0);
    lv_obj_set_size(demoBack_, 29, 25);
    lv_obj_align(demoBack_, LV_ALIGN_RIGHT_MID, -6, 0);

    auto *body = lv_obj_create(window);
    lv_obj_remove_style_all(body);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(body, 0, 35);
    lv_obj_set_size(body, width-2, height-37);
    if (desktopMonitor_) lv_obj_move_foreground(desktopMonitor_);
    if (StartWindowAnimation(window, false, this, AppWindowOpened))
        BoostWindowTimers();
    else
        SetWindowScale(window, WindowScaleNormal);
    return body;
}

void XDOSLVGL::BoostWindowTimers()
{
    if (windowTimersBoosted_) return;
    auto *refresh = lv_display_get_refr_timer(lv_obj_get_display(demoScreen_));
    auto *animation = lv_anim_get_timer();
    if (refresh) {
        windowRefreshPeriod_ = refresh->period;
        lv_timer_set_period(refresh, 16);
    }
    if (animation) {
        windowAnimationPeriod_ = animation->period;
        lv_timer_set_period(animation, 16);
    }
    windowTimersBoosted_ = true;
}

void XDOSLVGL::RestoreWindowTimers()
{
    if (!windowTimersBoosted_) return;
    auto *refresh = lv_display_get_refr_timer(lv_obj_get_display(demoScreen_));
    auto *animation = lv_anim_get_timer();
    if (refresh) lv_timer_set_period(refresh, windowRefreshPeriod_);
    if (animation) lv_timer_set_period(animation, windowAnimationPeriod_);
    windowTimersBoosted_ = false;
}

void XDOSLVGL::AppWindowOpened(lv_anim_t *animation)
{
    static_cast<XDOSLVGL *>(lv_anim_get_user_data(animation))->RestoreWindowTimers();
}

bool XDOSLVGL::CloseCurrentApp()
{
    if (!demoScreen_ || !demoBack_ || windowClosing_) return false;
    lv_obj_send_event(demoBack_, LV_EVENT_CLICKED, nullptr);
    return true;
}

void XDOSLVGL::BeginAppWindowClose()
{
    if (windowClosing_) return;
    windowClosing_ = true;
    auto *window = demoScreen_;
    lv_obj_add_state(window, LV_STATE_DISABLED);
    lv_obj_add_state(demoBack_, LV_STATE_DISABLED);
    lv_anim_delete(window, SetWindowScale);
    if (StartWindowAnimation(window, true, this, AppWindowClosed))
        BoostWindowTimers();
    else
        FinishAppWindowClose();
}

void XDOSLVGL::AppWindowClosed(lv_anim_t *animation)
{
    static_cast<XDOSLVGL *>(lv_anim_get_user_data(animation))->FinishAppWindowClose();
}

void XDOSLVGL::FinishAppWindowClose()
{
    RestoreWindowTimers();
    lv_obj_delete_async(demoScreen_);
    demoScreen_ = demoBack_ = nullptr;
    windowClosing_ = false;
    RestoreMonitor();
}
