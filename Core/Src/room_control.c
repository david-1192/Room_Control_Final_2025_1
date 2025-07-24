#include "main.h" // Para acceso a huart2
// Extern UART handle for debug
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3; // Para ESP-01

#include "room_control.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <string.h>
#include <stdio.h>
#include "led.h"
extern TIM_HandleTypeDef htim3; // Extern TIM handle for PWM fan control 

// Default password
static const char DEFAULT_PASSWORD[] = "A123";

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
static uint8_t map_fan_level_to_brightness(fan_level_t level);

/**
 * @brief Limpia el buffer de entrada y el índice
 * @param room Puntero a la estructura de control de la habitación
 */
void room_control_init(room_control_t *room) {
    // Initialize room control structure
    room->current_state = ROOM_STATE_LOCKED;
    strcpy(room->password, DEFAULT_PASSWORD); // Solo al arrancar
    room_control_clear_input(room);
    room->last_input_time = 0;
    room->state_enter_time = HAL_GetTick();
    
    // Initialize door control
    room->door_locked = true;
    
    // Initialize temperature and fan
    room->current_temperature = 22.0f;  // Default room temperature
    room->current_fan_level = FAN_LEVEL_OFF;
    room->manual_fan_override = false;
    
    // Display
    room->display_update_needed = true;
    
    // Inicializar hardware (door lock, fan PWM, etc.)
    HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_RESET);
    // Iniciar PWM del ventilador (TIM3, canal 1)
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}
/**
 * @brief Actualiza el estado de la habitación
 * @param room Puntero a la estructura de control de la habitación
 */
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
            // Mostrar todos los caracteres digitados como asteriscos
            for (uint8_t i = 0; i < room->input_index; i++) {
                room->display_buffer[i] = '*';
            }
            room->display_buffer[room->input_index] = '\0';
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
        default:
            break;

    }
    
    
    // Update subsystems
    room_control_update_door(room);
    room_control_update_fan(room);
    
    // Actualiza el brillo del LED según el nivel actual del ventilador
    uint8_t led_brightness = map_fan_level_to_brightness(room->current_fan_level);
    set_led_brightness(room->led, led_brightness);

    if (room->display_update_needed) {
        room_control_update_display(room);
        room->display_update_needed = false;
    }
}


// Elimina todos los espacios de una cadena (in-place)
static void remove_spaces(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src != ' ') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

/**
 * @brief Procesa una tecla ingresada por el usuario
 * @param room Puntero a la estructura de control de la habitación
 * @param key Tecla ingresada
 */
void room_control_process_key(room_control_t *room, char key) {
    room->last_input_time = HAL_GetTick();

    switch (room->current_state) {
        case ROOM_STATE_LOCKED:
            room_control_clear_input(room);
            if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'D')) {
                room->input_buffer[0] = key;
                room->input_index = 1;
            } else {
                room->input_index = 0;
            }
            room_control_change_state(room, ROOM_STATE_INPUT_PASSWORD);
            break;

        case ROOM_STATE_INPUT_PASSWORD:
            if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'D')) {
                if (room->input_index < 16) { //Permite hasta 16 caracteres
                    room->input_buffer[room->input_index] = key;
                    room->input_index++;
                }
            } else if (key == '#') {
                // Solo compara si se ingresaron exactamente PASSWORD_LENGTH caracteres
                if (room->input_index == PASSWORD_LENGTH) {
                    room->input_buffer[room->input_index] = '\0';
                    char input_clean[PASSWORD_LENGTH + 1];
                    strncpy(input_clean, room->input_buffer, PASSWORD_LENGTH);
                    input_clean[PASSWORD_LENGTH] = '\0';
                    remove_spaces(input_clean);

                    char password_clean[PASSWORD_LENGTH + 1];
                    strncpy(password_clean, room->password, PASSWORD_LENGTH);
                    password_clean[PASSWORD_LENGTH] = '\0';
                    remove_spaces(password_clean);

                    if (strcmp(input_clean, password_clean) == 0) {
                        room_control_change_state(room, ROOM_STATE_UNLOCKED);
                    } else {
                        room_control_change_state(room, ROOM_STATE_ACCESS_DENIED);
                    }
                } else {
                    // Si no se ingresaron 4 caracteres, acceso denegado
                    room_control_change_state(room, ROOM_STATE_ACCESS_DENIED);
                }
                room_control_clear_input(room);
            } else if (key == '*') {
                room_control_change_state(room, ROOM_STATE_LOCKED);
                room_control_clear_input(room);
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

/**
 * @brief Establece la temperatura actual y actualiza el ventilador si es necesario
 * @param room Puntero a la estructura de control de la habitación
 * @param temperature Nueva temperatura a establecer
 */
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


/**
 * @brief Fuerza el nivel del ventilador manualmente
 * @param room Puntero a la estructura de control de la habitación
 */
bool room_control_force_fan(room_control_t *room, int level) {
    if (level >= 0 && level <= 3) {
        room->manual_fan_override = true;
        switch (level) {
            case 0: room->current_fan_level = FAN_LEVEL_OFF; break;
            case 1: room->current_fan_level = FAN_LEVEL_LOW; break;
            case 2: room->current_fan_level = FAN_LEVEL_MED; break;
            case 3: room->current_fan_level = FAN_LEVEL_HIGH; break;
        }
        room->display_update_needed = true;
        return true;
    }
    return false;
}

void room_control_force_fan_level(room_control_t *room, fan_level_t level) {
    // Implementa la lógica para forzar el nivel del ventilador
    // Por ejemplo:
    room_control_force_fan(room, (int)level);
}

/**
 * @brief Cambia la contraseña del sistema
 * @param room Puntero a la estructura de control de la habitación
 * @param new_password Nueva contraseña a establecer
 * @return true si la contraseña se cambió correctamente, false en caso contrario
 */
bool room_control_change_password(room_control_t *room, const char *new_password) {
    if (strlen(new_password) == PASSWORD_LENGTH) {
        strcpy(room->password, new_password);
        return true;
    }
    return false;
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

/**
 * @brief Cambia el estado de la habitación
 * @param room Puntero a la estructura de control de la habitación
 * @param new_state Nuevo estado a establecer
 */
static void room_control_change_state(room_control_t *room, room_state_t new_state) {
    room->current_state = new_state;
    room->state_enter_time = HAL_GetTick();
    room->display_update_needed = true;


    switch (new_state) {
        case ROOM_STATE_LOCKED:
            room->door_locked = true;
            room_control_clear_input(room);
            // Apaga el indicador de acceso
            HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_RESET);

            // Apaga el override manual y pone el ventilador en automático
            room->manual_fan_override = false;
            room->current_fan_level = room_control_calculate_fan_level(room->current_temperature);
            room_control_update_fan(room); // Actualiza el hardware del ventilador
            break;

        case ROOM_STATE_UNLOCKED:
            room->door_locked = false;
            room->manual_fan_override = false;  // Reset manual override
            // Enciende el indicador de acceso
            HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_SET);
            break;

        case ROOM_STATE_ACCESS_DENIED:
            room_control_clear_input(room);
            // Apaga el indicador de acceso
            HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_RESET);

            // Enviar alerta por ESP-01 (USART3)

            char alert_msg[] = "POST /alert HTTP/1.1\r\nHost: mi-servidor.com\r\n\r\nAcceso denegado detectado\r\n";
            HAL_UART_Transmit(&huart3, (uint8_t*)alert_msg, strlen(alert_msg), 1000);
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

    switch (room->current_state) {
        case ROOM_STATE_LOCKED: {
            ssd1306_SetCursor(10, 10);
            ssd1306_WriteString("SISTEMA", Font_11x18, White);
            ssd1306_SetCursor(10, 30);
            ssd1306_WriteString("BLOQUEADO", Font_11x18, White);
            break;
        }
        case ROOM_STATE_INPUT_PASSWORD: {
            ssd1306_SetCursor(10, 10);
            ssd1306_WriteString("CLAVE: ", Font_11x18, White);
            ssd1306_SetCursor(10, 30);
            ssd1306_WriteString(room->display_buffer, Font_11x18, White);
            break;
        }
        case ROOM_STATE_UNLOCKED: {
            char temp_str[24];
            snprintf(temp_str, sizeof(temp_str), "Temp: %d C", (int)(room->current_temperature));
            ssd1306_SetCursor(5, 10);
            ssd1306_WriteString(temp_str, Font_11x18, White);

            // Mostrar el nivel forzado si está activo, si no, el calculado
            int nivel_a_mostrar = room->manual_fan_override ? room->current_fan_level : room_control_calculate_fan_level(room->current_temperature);
            char fan_str[32];
            snprintf(fan_str, sizeof(fan_str), "FAN: %d", nivel_a_mostrar);
            ssd1306_SetCursor(5, 35);
            ssd1306_WriteString(fan_str, Font_11x18, White);
            break;
        }
        case ROOM_STATE_ACCESS_DENIED: {
            ssd1306_SetCursor(5, 10);
            ssd1306_WriteString("ACCESO", Font_11x18, White);
            ssd1306_SetCursor(5, 35);
            ssd1306_WriteString("DENEGADO", Font_11x18, White);
            break;
        }
        default:
            break;
    }

    ssd1306_UpdateScreen();
}

/**
 * @brief Actualiza el estado de la puerta
 * @param room Puntero a la estructura de control de la habitación
 */
static void room_control_update_door(room_control_t *room) {
    if (room->door_locked) {
        HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_SET);
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

/**
 * @brief Establece el estado de la habitación y actualiza el ventilador
 * @param room Puntero a la estructura de control de la habitación
 */
void room_control_set_state(room_control_t *room, room_state_t new_state) {
    room->current_state = new_state;
    if (new_state == ROOM_STATE_LOCKED) {
        // Restaurar el ventilador a modo automático y desactivar forzado
        room->manual_fan_override = false;
        fan_level_t auto_level = room_control_calculate_fan_level(room->current_temperature);
        room->current_fan_level = auto_level;
        room_control_force_fan_level(room, auto_level);
    }
}

static uint8_t map_fan_level_to_brightness(fan_level_t level) {
    switch (level) {
        case FAN_LEVEL_OFF:  return 0;    // LED apagado
        case FAN_LEVEL_LOW:  return 30;   // Brillo bajo
        case FAN_LEVEL_MED:  return 70;   // Brillo medio
        case FAN_LEVEL_HIGH: return 100;  // Brillo máximo
        default: return 0;
    }
}