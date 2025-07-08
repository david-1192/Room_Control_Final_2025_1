#include "main.h" // Para acceso a huart2
// Extern UART handle for debug
extern UART_HandleTypeDef huart2;
#include "room_control.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <string.h>
#include <stdio.h>
#include "led.h"
extern led_handle_t led_access; // Uso del LD2 
extern TIM_HandleTypeDef htim3; // Extern TIM handle for PWM fan control 

// Default password
static const char DEFAULT_PASSWORD[] = "1234";

// Temperature thresholds for automatic fan control
// static const float TEMP_THRESHOLD_MED = 28.0f;
// static const float TEMP_THRESHOLD_HIGH = 31.0f;

// Timeouts in milliseconds
static const uint32_t INPUT_TIMEOUT_MS = 10000;  // 10 seconds
static const uint32_t ACCESS_DENIED_TIMEOUT_MS = 3000;  // 3 seconds

// Private function prototypes
static void room_control_change_state(room_control_t *room, room_state_t new_state);
static void room_control_update_display(room_control_t *room);
static void room_control_update_door(room_control_t *room);
static void room_control_update_fan(room_control_t *room);
static fan_level_t room_control_calculate_fan_level(float temperature);
static void room_control_clear_input(room_control_t *room);

static uint8_t access_led_active = 0;

/**
 * @brief Limpia el buffer de entrada y el índice
 * @param room Puntero a la estructura de control de la habitación
 */
void room_control_init(room_control_t *room) {
    // Initialize room control structure
    room->current_state = ROOM_STATE_LOCKED;
    strcpy(room->password, DEFAULT_PASSWORD);
    room_control_clear_input(room);
    room->last_input_time = 0;
    room->state_enter_time = HAL_GetTick();
    
    // Initialize door control
    // room->door_locked = true;
    
    // Initialize temperature and fan
    room->current_temperature = 22.0f;  // Default room temperature
    room->current_fan_level = FAN_LEVEL_OFF;
    room->manual_fan_override = false;
    
    // Display
    room->display_update_needed = true;
    
    // Inicializar hardware (door lock, fan PWM, etc.)
    // HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_RESET);
    // Iniciar PWM del ventilador (TIM3, canal 1)
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}
void room_control_update(room_control_t *room) {
    uint32_t current_time = HAL_GetTick();
    
    // State machine
    switch (room->current_state) {
        case ROOM_STATE_LOCKED:
            
            break;

        case ROOM_STATE_INPUT_PASSWORD: {
            // Timeout para volver a LOCKED
            if (current_time - room->last_input_time > INPUT_TIMEOUT_MS) {
                room_control_change_state(room, ROOM_STATE_LOCKED);
            }
            //  Convierte el buffer de entrada a asteriscos para mostrar en display
            for (uint8_t i = 0; i < PASSWORD_LENGTH; i++) {
                if (i < room->input_index) {
                    room->display_buffer[i] = '*';
                } else {
                    room->display_buffer[i] = '\0';
                }
            }
            room->display_buffer[PASSWORD_LENGTH] = '\0';
            break;
        }

        case ROOM_STATE_UNLOCKED:
            // Actualización de display para que el mensaje cambie automáticamente
            room->display_update_needed = true;
            break;

        case ROOM_STATE_ACCESS_DENIED:
            if (current_time - room->state_enter_time > ACCESS_DENIED_TIMEOUT_MS) {
                room_control_change_state(room, ROOM_STATE_LOCKED);
            }
            break;

        case ROOM_STATE_EMERGENCY:
            
            break;
    }
    
    
    // Update subsystems
    room_control_update_door(room);
    room_control_update_fan(room);
    
    if (room->display_update_needed) {
        room_control_update_display(room);
        room->display_update_needed = false;
    }
}

void room_control_process_key(room_control_t *room, char key) {
    room->last_input_time = HAL_GetTick();
    
    switch (room->current_state) {
        case ROOM_STATE_LOCKED:
            // Start password input
            room_control_clear_input(room);
            room->input_buffer[0] = key;
            room->input_index = 1;
            room_control_change_state(room, ROOM_STATE_INPUT_PASSWORD);
            break;

        case ROOM_STATE_INPUT_PASSWORD:
            // Solo aceptar dígitos 
            if (room->input_index < PASSWORD_LENGTH && key >= '0' && key <= '9') {
                room->input_buffer[room->input_index] = key;
                room->input_index++;
            }
            // Si ya se ingresaron 4 dígitos, validar
            if (room->input_index >= PASSWORD_LENGTH) {
                room->input_buffer[PASSWORD_LENGTH] = '\0';
                if (strcmp(room->input_buffer, room->password) == 0) {
                    room_control_change_state(room, ROOM_STATE_UNLOCKED);
                } else {
                    room_control_change_state(room, ROOM_STATE_ACCESS_DENIED);
                }
            }
            break;

        case ROOM_STATE_UNLOCKED:
            // Tecla '*' para volver a bloquear
            if (key == '*') {
                room_control_change_state(room, ROOM_STATE_LOCKED);
            }
            break;

        default:
            break;
    }

    room->display_update_needed = true;
}

void room_control_set_temperature(room_control_t *room, float temperature) {
    room->current_temperature = temperature;
    
    // Actualizar el fan automáticamente si no hay override manual
    if (!room->manual_fan_override) {
        fan_level_t new_level = room_control_calculate_fan_level(temperature);
        if (new_level != room->current_fan_level) {
            room->current_fan_level = new_level;
            room->display_update_needed = true;
        }
    }
}

void room_control_force_fan_level(room_control_t *room, fan_level_t level) {
    room->manual_fan_override = true;
    room->current_fan_level = level;
    room->display_update_needed = true;
}

void room_control_change_password(room_control_t *room, const char *new_password) {
    if (strlen(new_password) == PASSWORD_LENGTH) {
        strcpy(room->password, new_password);
    }
}

// Status getters
room_state_t room_control_get_state(room_control_t *room) {
    return room->current_state;
}

bool room_control_is_door_locked(room_control_t *room) {
    return room->door_locked;
}

fan_level_t room_control_get_fan_level(room_control_t *room) {
    return room->current_fan_level;
}

float room_control_get_temperature(room_control_t *room) {
    return room->current_temperature;
}

// Private functions
static void room_control_change_state(room_control_t *room, room_state_t new_state) {
    room->current_state = new_state;
    room->state_enter_time = HAL_GetTick();
    room->display_update_needed = true;



    // State entry actions
    switch (new_state) {
        case ROOM_STATE_LOCKED:
            room->door_locked = true;
            room_control_clear_input(room);
            // Apaga el LED de acceso si estaba encendido
            led_off(&led_access); 
            access_led_active = 0;
            break;

        case ROOM_STATE_UNLOCKED:
            room->door_locked = false;
            room->manual_fan_override = false;  // Reset manual override
            break;

        case ROOM_STATE_ACCESS_DENIED:
            room_control_clear_input(room);
            // Apaga el LED de acceso si estaba encendido
            led_off(&led_access);
            access_led_active = 0;
            break;

        default:
            break;
    }
}

/**
 * @brief Actualiza la pantalla OLED según el estado actual de la habitación
 * @param room Puntero a la estructura de control de la habitación
 */
static void room_control_update_display(room_control_t *room) {
    
    ssd1306_Fill(Black);
    
    // Actualización de pantalla según estado
    switch (room->current_state) {
        case ROOM_STATE_LOCKED:
            ssd1306_SetCursor(10, 10);
            ssd1306_WriteString("SISTEMA", Font_11x18, White);
            ssd1306_SetCursor(10, 30);
            ssd1306_WriteString("BLOQUEADO", Font_11x18, White);
            break;
            

        case ROOM_STATE_INPUT_PASSWORD:
            ssd1306_SetCursor(10, 10);
            ssd1306_WriteString("CLAVE:", Font_11x18, White);
            ssd1306_SetCursor(10, 30);
            ssd1306_WriteString(room->display_buffer, Font_11x18, White);
            break;
            
        case ROOM_STATE_UNLOCKED: {
            // Debug: imprimir temperatura por UART2
            char debug_uart[32];
            snprintf(debug_uart, sizeof(debug_uart), "Temp: %d °C\r\n", (int)(room->current_temperature));
            HAL_UART_Transmit(&huart2, (uint8_t*)debug_uart, strlen(debug_uart), 100);
            uint32_t current_time = HAL_GetTick();
            // Mostrar "ACCESO CONCEDIDO" durante los primeros 3 segundos
            if (current_time - room->state_enter_time < 3000) {
                ssd1306_SetCursor(10, 5);
                ssd1306_WriteString("ACCESO", Font_11x18, White);
                ssd1306_SetCursor(10, 25);
                ssd1306_WriteString("CONCEDIDO", Font_11x18, White);
                led_on(&led_access);
            } else {
                // Después de 3 segundos, mostrar temperatura y fan nivel/porcentaje
                char temp_str[24];
                snprintf(temp_str, sizeof(temp_str), "Temp: %d C", (int)(room->current_temperature));
                ssd1306_SetCursor(5, 10);
                ssd1306_WriteString(temp_str, Font_11x18, White);
                led_off(&led_access);

                int fan_level = 0;
                if (room->current_temperature < 25.0f) {
                    fan_level = 0;
                } else if (room->current_temperature < 28.0f) {
                    fan_level = 1;
                } else if (room->current_temperature < 31.0f) {
                    fan_level = 2;
                } else {
                    fan_level = 3;
                }

                char fan_str[32];
                snprintf(fan_str, sizeof(fan_str), "NIVEL: %d ", fan_level);
                ssd1306_SetCursor(5, 35);
                ssd1306_WriteString(fan_str, Font_11x18, White);
            }
            break;
        }
            
        case ROOM_STATE_ACCESS_DENIED:
            ssd1306_SetCursor(10, 10);
            ssd1306_WriteString("ACCESO", Font_11x18, White);
            ssd1306_SetCursor(10, 30);
            ssd1306_WriteString("DENEGADO", Font_11x18, White);
            break;
            
        default:
            break;
    }
    
    ssd1306_UpdateScreen();
}

static void room_control_update_door(room_control_t *room) {
    // TODO: TAREA - Implementar control físico de la puerta
    // Ejemplo usando el pin DOOR_STATUS:
    if (room->door_locked) {
        // HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_RESET);
    } else {
        // HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_SET);
    }
}

/**
 * @brief Actualiza el nivel del ventilador basado en la temperatura actual
 * @param room Puntero a la estructura de control de la habitación
 */
static void room_control_update_fan(room_control_t *room) {
    // Control PWM del ventilador
    uint32_t pwm_value = (room->current_fan_level * 99) / 100;  // 0-99 para period=99
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_value);
}

/**
 * @brief Calcula el nivel de ventilador basado en la temperatura
 * @param temperature Temperatura actual en grados Celsius
 */
static fan_level_t room_control_calculate_fan_level(float temperature) {
    // Nivel 0 (0%): Temp < 25°C
    // Nivel 1 (30%): 25°C ≤ Temp < 28°C
    // Nivel 2 (70%): 28°C ≤ Temp < 31°C
    // Nivel 3 (100%): Temp ≥ 31°C
    if (temperature < 25.0f) {
        return FAN_LEVEL_OFF;
    } else if (temperature < 28.0f) {
        return FAN_LEVEL_LOW;
    } else if (temperature < 31.0f) {
        return FAN_LEVEL_MED;
    } else {
        return FAN_LEVEL_HIGH;
    }
}

static void room_control_clear_input(room_control_t *room) {
    memset(room->input_buffer, 0, sizeof(room->input_buffer));
    room->input_index = 0;
}