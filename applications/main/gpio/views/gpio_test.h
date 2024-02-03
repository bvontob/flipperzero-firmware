#pragma once

#include "../gpio_items.h"

#include <gui/view.h>
#include <notification/notification_messages.h>

typedef struct GpioTest GpioTest;
typedef void (*GpioTestOkCallback)(InputType type, void* context);

GpioTest* gpio_test_alloc(GPIOItems* gpio_items, NotificationApp* notifications);

void gpio_test_free(GpioTest* gpio_test);

View* gpio_test_get_view(GpioTest* gpio_test);

void gpio_test_set_ok_callback(GpioTest* gpio_test, GpioTestOkCallback callback, void* context);
