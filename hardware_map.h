#pragma once

#define VALVE_PINS_COUNT 3

// Pines MOSFET (salidas)
const int VALVE_PINS[3] = {27, 26, 25};

// Canales ADC (entradas humedad)
const int SENSOR_CHANNELS[3] = {0, 1, 3};

// Canales ADC corriente
const int CURRENT_CHANNELS[3] = {4, 5, 6};
