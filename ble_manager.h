//ble_manager.h
#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include "globals.h"
#include <NimBLEDevice.h>

// Nombre del dispositivo BLE
#define DEVICE_NAME "ESP32-SETUP"

// UUIDs del servicio y characteristic
extern const char* WIFI_SERVICE_UUID;
extern const char* WIFI_CHARACTERISTIC_UUID;

// Inicializa todo el BLE
void BLE_init();

#endif
