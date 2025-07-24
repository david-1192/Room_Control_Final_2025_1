#pragma once
#include "room_control.h"
#include "stm32l4xx_hal.h"

void command_parser_process_esp01(uint8_t rx_byte);
void command_parser_process_debug(uint8_t rx_byte);
void command_parser_process(room_control_t *room, const char *cmd, UART_HandleTypeDef *huart);