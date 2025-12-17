
//globals.h
#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <vector>
#include "run_types.h"
#include <Adafruit_MCP3008.h>
#include <time.h>

#define MAX_CLIMA_HORAS 5

struct ClimaHora {
    String time;              // "YYYY-MM-DD HH:00"
    uint8_t rain_probability; // 0–100
};

extern ClimaHora climaHoras[MAX_CLIMA_HORAS];
extern uint8_t climaHorasCount;
extern unsigned long lastWeatherUpdate;

extern String wifi_ssid;
extern String wifi_pass;
extern String id_device;
extern std::vector<ValveRuntime> runtime;
extern Adafruit_MCP3008 adc;

struct DaysOfWeek {
    bool monday_on;
    bool tuesday_on;
    bool wednesday_on;
    bool thursday_on;
    bool friday_on;
    bool saturday_on;
    bool sunday_on;
};
struct ScheduleConfig {
    String modo_frecuencia;   // "INTERVAL_DAYS" | "SELECTED_DAYS"
    int interval_days;
    int duration;             // segundos de riego
    DaysOfWeek days_of_week;
    std::vector<String> water_times; // ["19:00", "14:15", ...]
};
struct ValvulaConfig {
    int id_valvula;

    bool is_active;
    bool weather_enable;
    bool schedule_enable;
    bool sensor_enable;
    bool manual_watering;

    unsigned char humidity_min;
    unsigned char humidity_max;

    String modo_control;  // "SENSOR" | "SCHEDULED"

    ScheduleConfig schedule;
};

struct EquipoInfo {
    int cantidad_valvulas;
    int cantidad_sensores;
};

struct ConfigSistema {
    int status;
    EquipoInfo equipo;
    std::vector<ValvulaConfig> valvulas;
};

extern ConfigSistema config;

void cargarConfigDePrefs();
void guardarConfigEnPrefs();

//Funciones para leer y escribir el prefs con las credenciales del wifi
void cargarWifi();
void guardarWifi(String red, String pass);
bool obtenerTiempo(struct tm &tinfo);

#endif
