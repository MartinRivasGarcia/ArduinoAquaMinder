#pragma once
#include <Arduino.h>
#include <vector>
#include <time.h>
#include <Preferences.h>
#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

void onConfigUpdated();
// ----------------------------
// TIPOS DE MODO DE RIEGO
// ----------------------------
enum WaterMode {
    MANUAL_MODE = 0,
    SENSOR_MODE = 1,
    SELECTED_DAYS = 2,
    INTERVAL_DAYS = 3
};

// Forward declarations
bool scheduleShouldStartNow(int valveIndex);
bool intervalShouldStartNow(int valveIndex, time_t ahora);
time_t getNextWatering(int valveIndex);

void initSchedule();


void scheduleManagerUpdate(int i);
void clearManualOriginFlag(int i);  // opcional si lo usás desde schedule_manager

#endif