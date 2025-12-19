#include "globals.h"
#include "pedidosHTTP.h"
#include <Preferences.h>
#include <ArduinoJson.h>

Preferences prefs;
ConfigSistema config;
std::vector<ValveRuntime> runtime;
Adafruit_MCP3008 adc;

String wifi_ssid = "";
String wifi_pass = "";

String id_device = "123678";

ClimaHora climaHoras[MAX_CLIMA_HORAS];
uint8_t climaHorasCount = 0;
unsigned long lastWeatherUpdate = 0;

//Guardar wifi en prefs
void guardarWifi(String red, String pass) {
  prefs.begin("wifi", false);
  prefs.putString("red", red);
  prefs.putString("pass", pass);
  prefs.end();
}

void cargarWifi() {
  prefs.begin("wifi", true);
  wifi_ssid = prefs.getString("red", "");
  wifi_pass = prefs.getString("pass", "");
  //wifi_ssid = "MartinYOrne";
  //wifi_pass = "centroteca294";
 // wifi_ssid = "Android-GC";
  //wifi_pass = "12345678";
  prefs.end();
}


void guardarConfigEnPrefs() {
    prefs.begin("riego", false);  // namespace

    // Guardar config completo como JSON compactado
    StaticJsonDocument<4096> doc;

    // Equipo:
    doc["id_device"] = id_device;
    doc["status"] = config.status;
    doc["equipo"]["cantidad_valvulas"] = config.equipo.cantidad_valvulas;
    doc["equipo"]["cantidad_sensores"] = config.equipo.cantidad_sensores;

    JsonArray arr = doc.createNestedArray("valvulas");

    for (auto &v : config.valvulas) {
        JsonObject o = arr.createNestedObject();
        o["id_valvula"] = v.id_valvula;
        o["is_active"] = v.is_active ? 1 : 0;
        o["weather_enable"] = v.weather_enable ? 1 : 0;
        o["schedule_enable"] = v.schedule_enable ? 1 : 0;
        o["sensor_enable"] = v.sensor_enable ? 1 : 0;
        o["humidity_min"] = v.humidity_min;
        o["humidity_max"] = v.humidity_max;
        o["modo_control"] = v.modo_control;
        o["manual_watering"] = v.manual_watering ? 1 : 0;

        JsonObject s = o.createNestedObject("schedule");
        s["modo_frecuencia"] = v.schedule.modo_frecuencia;
        s["interval_days"] = v.schedule.interval_days;
        s["duration"] = v.schedule.duration;

        JsonObject d = s.createNestedObject("days_of_week");
        d["monday_on"] = v.schedule.days_of_week.monday_on ? 1 : 0;
        d["tuesday_on"] = v.schedule.days_of_week.tuesday_on ? 1 : 0;
        d["wednesday_on"] = v.schedule.days_of_week.wednesday_on ? 1 : 0;
        d["thursday_on"] = v.schedule.days_of_week.thursday_on ? 1 : 0;
        d["friday_on"] = v.schedule.days_of_week.friday_on ? 1 : 0;
        d["saturday_on"] = v.schedule.days_of_week.saturday_on ? 1 : 0;
        d["sunday_on"] = v.schedule.days_of_week.sunday_on ? 1 : 0;

        JsonArray ts = s.createNestedArray("water_times");
        for (auto &t : v.schedule.water_times) ts.add(t);
    }

    String out;
    serializeJson(doc, out);

    prefs.putString("cfg", out);

    prefs.end();
}

void cargarConfigDePrefs() {
    prefs.begin("riego", true);

    String json = prefs.getString("cfg", "");
    prefs.end();

    if (json.length() == 0) {
        Serial.println("⚠ No había config guardada en prefs.");
        return;
    }

    Serial.println("Cargando config desde prefs...");
    parseConfigJSON(json, false);
}

bool obtenerTiempo(struct tm &tinfo) {
    if (!getLocalTime(&tinfo)) {
        Serial.println("ERROR: No se pudo obtener tiempo actual");
        return false;
    }
    return true;
}
