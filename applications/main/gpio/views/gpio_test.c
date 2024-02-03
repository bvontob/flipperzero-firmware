#include "gpio_test.h"
#include "../gpio_items.h"

#include <gui/elements.h>

struct GpioTest {
    View* view;
    GpioTestOkCallback callback;
    void* context;
};

typedef struct {
    uint8_t pin_idx;
    GPIOItems* gpio_items;
} GpioTestModel;

static bool gpio_test_process_left(GpioTest* gpio_test);
static bool gpio_test_process_right(GpioTest* gpio_test);
static bool gpio_test_process_updown(GpioTest* gpio_test, InputEvent* event);
static bool gpio_test_process_ok(GpioTest* gpio_test, InputEvent* event);

static void gpio_test_draw_pin(
    Canvas* canvas,
    const int x,
    const int y,
    const char* name,
    const bool on,
    const bool out,
    const bool sel) {
    const int w = 6;
    const int m = w / 2;

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, x + m, y, AlignCenter, AlignBottom, name);

    for(int i = 0; i < 3; i++) {
        const int ya = out ? (y + 5 - i) : (y + 3 + i);
        canvas_draw_line(canvas, x + i, ya, x + 5 - i, ya);
    }

    if(on) {
        canvas_draw_box(canvas, x, y + 8, w, w);
    } else {
        canvas_draw_frame(canvas, x, y + 8, w, w);
    }

    if(sel) {
        canvas_draw_line(canvas, x - 4, y - 10, x + w + 4, y - 10);
        canvas_draw_line(canvas, x - 4, y + 16, x + w + 4, y + 16);
    }
}

static void gpio_test_draw_callback(Canvas* canvas, void* _model) {
    GpioTestModel* model = _model;
    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 64, 2, AlignCenter, AlignTop, "GPIO Manual Control");
    canvas_set_font(canvas, FontSecondary);
    if(model->pin_idx) elements_button_left(canvas, model->pin_idx ? "" : "Config");
    if(gpio_items_pin_is_output(model->gpio_items, model->pin_idx))
        elements_button_center(canvas, "High");
    if(model->pin_idx < gpio_items_get_count(model->gpio_items))
        elements_button_right(
            canvas, model->pin_idx < gpio_items_get_count(model->gpio_items) - 1 ? "" : "All");

    int x = 4;
    const int y = 28;

    for(int p = 0; p < gpio_items_get_count(model->gpio_items); p++) {
        if(p == 6) x += 9;
        gpio_test_draw_pin(
            canvas,
            x,
            y,
            gpio_items_get_pin_short_name(model->gpio_items, p),
            gpio_items_pin_is_high(model->gpio_items, p),
            gpio_items_pin_is_output(model->gpio_items, p),
            p == model->pin_idx || model->pin_idx == gpio_items_get_count(model->gpio_items));
        x += 15;
    }
}

static bool gpio_test_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    GpioTest* gpio_test = context;
    bool consumed = false;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyRight) {
            consumed = gpio_test_process_right(gpio_test);
        } else if(event->key == InputKeyLeft) {
            consumed = gpio_test_process_left(gpio_test);
        } else if(event->key == InputKeyUp || event->key == InputKeyDown) {
            consumed = gpio_test_process_updown(gpio_test, event);
        }
    } else if(event->key == InputKeyOk) {
        consumed = gpio_test_process_ok(gpio_test, event);
    }

    return consumed;
}

static bool gpio_test_process_left(GpioTest* gpio_test) {
    with_view_model(
        gpio_test->view,
        GpioTestModel * model,
        {
            if(model->pin_idx) {
                model->pin_idx--;
            }
        },
        true);
    return true;
}

static bool gpio_test_process_right(GpioTest* gpio_test) {
    with_view_model(
        gpio_test->view,
        GpioTestModel * model,
        {
            if(model->pin_idx < gpio_items_get_count(model->gpio_items)) {
                model->pin_idx++;
            }
        },
        true);
    return true;
}

static bool gpio_test_process_updown(GpioTest* gpio_test, InputEvent* event) {
    with_view_model(
        gpio_test->view,
        GpioTestModel * model,
        {
            if(model->pin_idx < gpio_items_get_count(model->gpio_items)) {
                if(event->key == InputKeyUp) {
                    if(gpio_items_pin_is_output(model->gpio_items, model->pin_idx)) {
                        gpio_items_set_pin(model->gpio_items, model->pin_idx, true);
                    } else {
                        gpio_items_configure_pin(
                            model->gpio_items, model->pin_idx, GpioModeOutputPushPull);
                    }
                } else {
                    if(gpio_items_pin_is_output(model->gpio_items, model->pin_idx) &&
                       gpio_items_pin_is_high(model->gpio_items, model->pin_idx)) {
                        gpio_items_set_pin(model->gpio_items, model->pin_idx, true);
                        gpio_items_set_pin(model->gpio_items, model->pin_idx, false);
                    } else {
                        gpio_items_configure_pin(model->gpio_items, model->pin_idx, GpioModeInput);
                    }
                }
            } else {
                bool all_outputs = true;
                bool some_high = false;
                for(int i = 0; i < gpio_items_get_count(model->gpio_items); i++) {
                    if(gpio_items_pin_is_output(model->gpio_items, i)) {
                        if(gpio_items_pin_is_high(model->gpio_items, i)) {
                            some_high = true;
                        }
                    } else {
                        all_outputs = false;
                    }
                }
                if(event->key == InputKeyUp) {
                    if(all_outputs) {
                        gpio_items_set_all_pins(model->gpio_items, true);
                    } else {
                        gpio_items_configure_all_pins(model->gpio_items, GpioModeOutputPushPull);
                    }
                } else {
                    if(some_high) {
                        gpio_items_set_all_pins(model->gpio_items, false);
                    } else {
                        gpio_items_configure_all_pins(model->gpio_items, GpioModeInput);
                    }
                }
            }
        },
        true);
    return true;
}

static bool gpio_test_process_ok(GpioTest* gpio_test, InputEvent* event) {
    bool consumed = false;

    with_view_model(
        gpio_test->view,
        GpioTestModel * model,
        {
            if(event->type == InputTypePress) {
                if(model->pin_idx < gpio_items_get_count(model->gpio_items)) {
                    gpio_items_set_pin(model->gpio_items, model->pin_idx, true);
                } else {
                    gpio_items_set_all_pins(model->gpio_items, true);
                }
                consumed = true;
            } else if(event->type == InputTypeRelease) {
                if(model->pin_idx < gpio_items_get_count(model->gpio_items)) {
                    gpio_items_set_pin(model->gpio_items, model->pin_idx, false);
                } else {
                    gpio_items_set_all_pins(model->gpio_items, false);
                }
                consumed = true;
            }
            gpio_test->callback(event->type, gpio_test->context);
        },
        true);

    return consumed;
}

GpioTest* gpio_test_alloc(GPIOItems* gpio_items) {
    GpioTest* gpio_test = malloc(sizeof(GpioTest));

    gpio_test->view = view_alloc();
    view_allocate_model(gpio_test->view, ViewModelTypeLocking, sizeof(GpioTestModel));

    with_view_model(
        gpio_test->view, GpioTestModel * model, { model->gpio_items = gpio_items; }, false);

    view_set_context(gpio_test->view, gpio_test);
    view_set_draw_callback(gpio_test->view, gpio_test_draw_callback);
    view_set_input_callback(gpio_test->view, gpio_test_input_callback);

    return gpio_test;
}

void gpio_test_free(GpioTest* gpio_test) {
    furi_assert(gpio_test);
    view_free(gpio_test->view);
    free(gpio_test);
}

View* gpio_test_get_view(GpioTest* gpio_test) {
    furi_assert(gpio_test);
    return gpio_test->view;
}

void gpio_test_set_ok_callback(GpioTest* gpio_test, GpioTestOkCallback callback, void* context) {
    furi_assert(gpio_test);
    furi_assert(callback);
    with_view_model(
        gpio_test->view,
        GpioTestModel * model,
        {
            UNUSED(model);
            gpio_test->callback = callback;
            gpio_test->context = context;
        },
        false);
}
