#ifndef VALVE_MANAGER_H
#define VALVE_MANAGER_H

#include "globals.h"
#include "pedidosHTTP.h"

// Inicializa runtime
void initValves();

// Lógica principal de cada válvula
void updateValve(int i);
void updateValveSensor(int i);
void updateValveScheduled(int i);

// Helpers
void handleSchedule(int i);
void handleWeather(int i);
void handleManualWatering(int i);

void iniciarRiego(int i);
void apagarValvula(int i);

int leerSensor(int i);
void saveHumedad (int humedad, int i);
bool shouldBlockIrrigationByWeather(uint8_t threshold);

#pragma once

#endif
