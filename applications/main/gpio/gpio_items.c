#include "gpio_items.h"

#include <furi_hal_resources.h>

typedef struct {
    const GpioPin* pin;
    const char* name;
    const char* short_name;
    uint8_t number;
    bool high;
    bool output;
} GPIOItem;

struct GPIOItems {
    GPIOItem* pins;
    size_t count;
};

GPIOItems* gpio_items_alloc() {
    GPIOItems* items = malloc(sizeof(GPIOItems));

    items->count = 0;
    for(size_t i = 0; i < gpio_pins_count; i++) {
        if(!gpio_pins[i].debug) {
            items->count++;
        }
    }

    items->pins = malloc(sizeof(GPIOItem) * items->count);
    size_t index = 0;
    for(size_t i = 0; i < gpio_pins_count; i++) {
        if(!gpio_pins[i].debug) {
            items->pins[index++] = (GPIOItem){
                .pin = gpio_pins[i].pin,
                .name = gpio_pins[i].name,
                .short_name = *gpio_pins[i].name != '\0' ? gpio_pins[i].name + 1 : "",
                .number = gpio_pins[i].number,
                .high = false,
                .output = false,
            };
        }
    }
    return items;
}

void gpio_items_free(GPIOItems* items) {
    free(items->pins);
    free(items);
}

uint8_t gpio_items_get_count(GPIOItems* items) {
    return items->count;
}

void gpio_items_configure_pin(GPIOItems* items, uint8_t index, GpioMode mode) {
    furi_assert(index < items->count);
    items->pins[index].output =
        (mode == GpioModeOutputPushPull || mode == GpioModeOutputOpenDrain);
    if(items->pins[index].output) gpio_items_set_pin(items, index, items->pins[index].high);
    furi_hal_gpio_init(
        items->pins[index].pin,
        mode,
        mode == GpioModeInput ? GpioPullDown : GpioPullNo,
        GpioSpeedVeryHigh);
}

void gpio_items_configure_all_pins(GPIOItems* items, GpioMode mode) {
    for(uint8_t i = 0; i < items->count; i++) {
        gpio_items_configure_pin(items, i, mode);
    }
}

void gpio_items_set_pin(GPIOItems* items, uint8_t index, bool level) {
    furi_assert(index < items->count);
    furi_hal_gpio_write(items->pins[index].pin, level);
    items->pins[index].high = level;
}

void gpio_items_set_all_pins(GPIOItems* items, bool level) {
    for(uint8_t i = 0; i < items->count; i++) {
        gpio_items_set_pin(items, i, level);
    }
}

const char* gpio_items_get_pin_name(GPIOItems* items, uint8_t index) {
    furi_assert(index < items->count + 1);
    if(index == items->count) {
        return "ALL";
    } else {
        return items->pins[index].name;
    }
}

const char* gpio_items_get_pin_short_name(GPIOItems* items, uint8_t index) {
    furi_assert(index < items->count + 1);
    if(index == items->count) {
        return "all";
    } else {
        return items->pins[index].short_name;
    }
}

bool gpio_items_pin_is_high(GPIOItems* items, uint8_t index) {
    if(items->pins[index].output)
        return items->pins[index].high;
    else
        return furi_hal_gpio_read(items->pins[index].pin);
}

bool gpio_items_pin_is_output(GPIOItems* items, uint8_t index) {
    return items->pins[index].output;
}
