#include "command_parser.h"
#include "room_control.h"
#include "temperature_sensor.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern room_control_t room_system;

#define CMD_BUFFER_SIZE 32

static char esp01_cmd_buffer[CMD_BUFFER_SIZE];
static uint8_t esp01_cmd_index = 0;
static char debug_cmd_buffer[CMD_BUFFER_SIZE];
static uint8_t debug_cmd_index = 0;

/**
 * @brief Procesa cada byte recibido por ESP-01 (USART3)
 * @param rx_byte Byte recibido
 */
void command_parser_process_esp01(uint8_t rx_byte) {
    if (rx_byte == '\n' || rx_byte == '\r') {
        esp01_cmd_buffer[esp01_cmd_index] = '\0';
        command_parser_process(&room_system, esp01_cmd_buffer, &huart3);
        esp01_cmd_index = 0;
    } else if (esp01_cmd_index < CMD_BUFFER_SIZE - 1) {
        esp01_cmd_buffer[esp01_cmd_index++] = rx_byte;
    }
}

/**
 * @brief Procesa cada byte recibido por debug (USART2)
 * @param rx_byte Byte recibido
 */
void command_parser_process_debug(uint8_t rx_byte) {
    
    if (rx_byte == '\n' || rx_byte == '\r') {
        debug_cmd_buffer[debug_cmd_index] = '\0';
        command_parser_process(&room_system, debug_cmd_buffer, &huart2);
        debug_cmd_index = 0;
    } else if (debug_cmd_index < CMD_BUFFER_SIZE - 1) {
        debug_cmd_buffer[debug_cmd_index++] = rx_byte;
    }
}

/**
 * @brief Procesa un comando recibido y ejecuta la acción correspondiente
 * @param room Puntero a la estructura de control de la habitación
 * @param cmd Cadena con el comando recibido
 * @param huart UART por la que se envía la respuesta (USART2 o USART3)
 */
void command_parser_process(room_control_t *room, const char *cmd, UART_HandleTypeDef *huart) {
    char tx_buffer[64];
    char clean_cmd[CMD_BUFFER_SIZE];
    strncpy(clean_cmd, cmd, CMD_BUFFER_SIZE - 1);
    clean_cmd[CMD_BUFFER_SIZE - 1] = '\0';

    // Elimina espacios y saltos de línea al final
    int len = strlen(clean_cmd);
    while (len > 0 && (clean_cmd[len-1] == '\r' || clean_cmd[len-1] == '\n' || clean_cmd[len-1] == ' ')) {
        clean_cmd[--len] = '\0';
    }

    // Ignora comandos vacíos
    if (strlen(clean_cmd) == 0) {
        return;
    }

    // Solo permite comandos si el sistema está desbloqueado
    if (room_control_get_state(room) != ROOM_STATE_UNLOCKED) {
        snprintf(tx_buffer, sizeof(tx_buffer), "SISTEMA BLOQUEADO\r\n"); // Mensaje de sistema bloqueado
        HAL_UART_Transmit(huart, (uint8_t*)tx_buffer, strlen(tx_buffer), 1000); 
        return; 
    }

    // Comando para obtener la temperatura actual
    if (strcmp(clean_cmd, "GET_TEMP") == 0) {
        int temp = (int)(temperature_sensor_read() + 0.5f); // Lee y redondea la temperatura
        snprintf(tx_buffer, sizeof(tx_buffer), "TEMP: %d C\r\n", temp); // Prepara respuesta
        HAL_UART_Transmit(huart, (uint8_t*)tx_buffer, strlen(tx_buffer), 1000); 

    // Comando para obtener el estado del sistema y nivel del ventilador
    } else if (strcmp(clean_cmd, "GET_STATUS") == 0) {
        snprintf(tx_buffer, sizeof(tx_buffer), "SYSTEM: %s\r\n", room_control_get_state(room) == ROOM_STATE_LOCKED ? "LOCKED" : "UNLOCKED");
        HAL_UART_Transmit(huart, (uint8_t*)tx_buffer, strlen(tx_buffer), 1000); // Estado del sistema
        snprintf(tx_buffer, sizeof(tx_buffer), "FAN: %d\r\n", room_control_get_fan_level(room));
        HAL_UART_Transmit(huart, (uint8_t*)tx_buffer, strlen(tx_buffer), 1000); // Nivel del ventilador

    // Comando para cambiar la contraseña
    } else if (strncmp(clean_cmd, "SET_PASS:", 9) == 0) {
        char new_pass[8];
        strncpy(new_pass, clean_cmd + 9, sizeof(new_pass) - 1); // Extrae la nueva contraseña
        new_pass[sizeof(new_pass) - 1] = '\0';
        int plen = strlen(new_pass);
        while (plen > 0 && (new_pass[plen-1] == '\r' || new_pass[plen-1] == '\n' || new_pass[plen-1] == ' ')) {
            new_pass[--plen] = '\0';
        }
        if (strlen(new_pass) == 4) { // Verifica longitud válida
            room_control_change_password(room, new_pass); // Cambia la contraseña
            snprintf(tx_buffer, sizeof(tx_buffer), "NEW PASS: %s\r\n", new_pass); // Respuesta exitosa
        } else {
            snprintf(tx_buffer, sizeof(tx_buffer), "INVALID PASSWORD\r\n"); // Respuesta de error
        }
        HAL_UART_Transmit(huart, (uint8_t*)tx_buffer, strlen(tx_buffer), 1000); 

    // Comando para forzar el nivel del ventilador
    } else if (strncmp(clean_cmd, "FORCE_FAN:", 10) == 0) {
        char arg[4];
        strncpy(arg, clean_cmd + 10, sizeof(arg) - 1); // Extrae argumento de nivel
        arg[sizeof(arg) - 1] = '\0';
        int alen = strlen(arg);
        while (alen > 0 && (arg[alen-1] == '\r' || arg[alen-1] == '\n' || arg[alen-1] == ' ')) {
            arg[--alen] = '\0';
        }
        int level = atoi(arg); // Convierte argumento a entero
        if (level >= 0 && level <= 3) { // Verifica rango válido
            room_control_force_fan_level(room, (fan_level_t)level); // Fuerza nivel del ventilador
            snprintf(tx_buffer, sizeof(tx_buffer), "FAN LEVEL %d\r\n", level); // Respuesta exitosa
        } else {
            snprintf(tx_buffer, sizeof(tx_buffer), "INVALID FAN LEVEL\r\n"); // Respuesta de error
        }
        HAL_UART_Transmit(huart, (uint8_t*)tx_buffer, strlen(tx_buffer), 1000); 

    // Comando desconocido
    } else {
        snprintf(tx_buffer, sizeof(tx_buffer), "UNKNOWN COMMAND\r\n"); // Respuesta de comando desconocido
        HAL_UART_Transmit(huart, (uint8_t*)tx_buffer, strlen(tx_buffer), 1000); 
    }
}