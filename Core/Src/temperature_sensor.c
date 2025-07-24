#include "temperature_sensor.h"
#include "main.h" // Para acceso a hadc1 si se usa ADC
#include "stm32l4xx_hal.h"
#include <math.h>

// Permite acceso al ADC global definido en main.c
extern ADC_HandleTypeDef hadc1;

void temperature_sensor_init(void) {
    // ya esta inicializado en main.c
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
    // Inicia la conversión ADC
    HAL_ADC_Start(&hadc1);
    // Espera a que la conversión termine
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
    // Obtiene el valor digital del ADC
    uint32_t adc_value = HAL_ADC_GetValue(&hadc1);
    // Detiene el ADC
    HAL_ADC_Stop(&hadc1);

    // Referencia de voltaje del sistema (Vref)
    float Vref = 3.3f;
    // Resistencia fija del divisor (10kΩ)
    float R_fixed = 10000.0f;
    // Valor máximo del ADC de 12 bits
    float adc_max = 4095.0f;

    // Calcula el voltaje de salida del divisor resistivo
    float Vout = (adc_value / adc_max) * Vref;

    // Calcula la resistencia del NTC usando la ecuación del divisor
    float R_ntc = R_fixed * (Vref / Vout - 1);

    // Parámetros del NTC: Beta, temperatura de referencia y resistencia de referencia
    float Beta = 3950.0f;
    float T0 = 298.15f; // 25°C en Kelvin
    float R0 = 10000.0f; // Resistencia a 25°C

    // Calcula la temperatura en Kelvin usando la ecuación de Beta
    float tempK = 1.0f / ( (1.0f / T0) + (1.0f / Beta) * log(R_ntc / R0) );
    // Convierte la temperatura a grados Celsius
    float tempC = tempK - 273.15f;

    // Devuelve la temperatura en grados Celsius
    return tempC;
}