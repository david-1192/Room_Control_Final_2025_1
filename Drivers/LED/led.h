#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include <stdint.h>
#include "main.h"

// Tipos de datos públicos
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} led_handle_t;

// API pública
void led_init(led_handle_t *led);
void led_on(led_handle_t *led);
void led_off(led_handle_t *led);
void led_toggle(led_handle_t *led);
void set_led_brightness(led_handle_t *led, uint8_t brightness);

#endif // LED_DRIVER_H