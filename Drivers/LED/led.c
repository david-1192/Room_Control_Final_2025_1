#include "led.h"
#include "main.h" // Para acceso a htim3

void led_init(led_handle_t *led) {
    HAL_GPIO_WritePin(led->port, led->pin, GPIO_PIN_RESET);
}

void led_on(led_handle_t *led) {
    HAL_GPIO_WritePin(led->port, led->pin, GPIO_PIN_SET);
}

void led_off(led_handle_t *led) {
    HAL_GPIO_WritePin(led->port, led->pin, GPIO_PIN_RESET);
}

void led_toggle(led_handle_t *led) {
    HAL_GPIO_TogglePin(led->port, led->pin);
}

void set_led_brightness(led_handle_t *led, uint8_t brightness) {
    // brightness: 0-100
    // PWM sobre FAN_PWM_Pin (TIM3, canal 1)
    extern TIM_HandleTypeDef htim3;
    uint32_t pwm_value = (brightness * 99) / 100; // Ajusta según el periodo configurado (100)
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_value);
}