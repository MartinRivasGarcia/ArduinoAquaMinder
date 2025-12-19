#ifndef PEDIDOSHTTP_H
#define PEDIDOSHTTP_H

#include "globals.h"

void getUsuarios();
void getConfig();

void parseConfigJSON(const String &jsonStr, bool is_backend);
void reportWateringStatus(int id_valvula, bool is_watering);
void reportHumidityLog(int id_valvula, float humedad, const struct tm &now);
void notificacion(String mensaje);
void reportRiego(
    int id_valvula,
    float humedad,
    bool se_rego,
    bool lluvia,
    const struct tm &now
);
String urlencode(String str);
void getWeatherForecast();
void parseWeatherJSON(const String &jsonStr);

#endif