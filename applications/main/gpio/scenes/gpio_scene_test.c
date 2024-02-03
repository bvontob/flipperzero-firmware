#include "../gpio_app_i.h"

void gpio_scene_test_on_enter(void* context) {
    furi_assert(context);
    GpioApp* app = context;
    gpio_items_configure_all_pins(app->gpio_items, GpioModeInput);
    view_dispatcher_switch_to_view(app->view_dispatcher, GpioAppViewGpioTest);
}

bool gpio_scene_test_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gpio_scene_test_on_exit(void* context) {
    furi_assert(context);
    GpioApp* app = context;
    gpio_items_configure_all_pins(app->gpio_items, GpioModeAnalog);
    notification_message(app->notifications, &sequence_reset_blue);
}
