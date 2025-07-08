
#include "temperature_sensor.h"
#include "main.h" // Para acceso a hadc1 si se usa ADC
#include "stm32l4xx_hal.h"
#include <math.h>

// Permite acceso al ADC global definido en main.c
extern ADC_HandleTypeDef hadc1;

void temperature_sensor_init(void) {
    // Ya está inicializado en main.c
}

/**
 * @brief Lee la temperatura del sensor conectado al ADC.
 * 
 * Esta función inicia una conversión ADC, espera a que se complete,
 * y luego convierte el valor ADC a grados Celsius.
 * 
 * @return float Temperatura en grados Celsius.
 */
float temperature_sensor_read(void) {
    
    // Leer el valor del ADC
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
    uint32_t adc_value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    float temperature = (adc_value * 50.0f) / 4095.0f; // Convertir ADC a voltaje (asumiendo Vref = 3.3V y 12 bits de resolución )
    return temperature; // Retornar el valor de temperatura en grados Celsius
}
