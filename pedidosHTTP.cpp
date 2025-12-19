#include "pedidosHTTP.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "schedule_manager.h"

#define USE_SERIAL Serial

const String ip_server = "http://200.58.105.167";

void getUsuarios(){
  
    HTTPClient http;

    USE_SERIAL.print("[HTTP] begin...\n");
    // configure traged server and url
    //http.begin("https://www.howsmyssl.com/a/check", ca); //HTTPS
    http.begin(ip_server + "/usuarios");  //HTTP

    USE_SERIAL.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        USE_SERIAL.println(payload);
      }
    } else {
      USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
}

void notificacion(String mensaje){
    HTTPClient http;

    String encodedMsg = urlencode(mensaje);

    String url = ip_server +
        "/probandoNotificacion?mensaje=" + encodedMsg +
        "&id_device=" + id_device;

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode <= 0) {
        Serial.printf("[HTTP] GET failed: %s\n",
                      http.errorToString(httpCode).c_str());
    } else {
        Serial.printf("[HTTP] GET code probando Notificacion: %d\n", httpCode);
    }

    http.end();
}

String urlencode(String str) {
    String encoded = "";
    char c;
    char code0;
    char code1;
    for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (isalnum(c)) {
            encoded += c;
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) code0 = c - 10 + 'A';
            encoded += '%';
            encoded += code0;
            encoded += code1;
        }
    }
    return encoded;
}


void getConfig() {
    HTTPClient http;


    String url = ip_server + "/configuracionEquipo?id_equipo=" + id_device;
    //Serial.println("[HTTP] GET configuration... " + url);
    http.begin(ip_server + "/configuracionEquipo?id_equipo=" + id_device); //HTTP

    http.setConnectTimeout(30000); 
    http.setTimeout(35000);    

    int httpCode = http.GET();

    if (httpCode <= 0) {
        Serial.printf("[HTTP] GET failed: %s\n", http.errorToString(httpCode).c_str());
        cargarConfigDePrefs();
        return;
    }

    Serial.printf("[HTTP] GET code: %d ", httpCode);

    if (httpCode != HTTP_CODE_OK) {
        Serial.println("Respuesta inesperada");
        cargarConfigDePrefs();
        return;
    }

    String payload = http.getString();
    Serial.println("RAW payload:");
    Serial.println(payload);

    // ------------------------------
    // PARSEAR SOLO PARA new_info
    // ------------------------------
    StaticJsonDocument<4096> doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
        Serial.printf("Error parseando JSON inicial: %s\n", err.f_str());
        return;
    }
    bool newInfo = doc["equipo"]["new_info"];

    if (!newInfo) {
        Serial.println("⚡ No hay información nueva → cargo desde PREFERENCES");
        cargarConfigDePrefs();
        http.end();
        return;
    }

    Serial.println("🔥 Nueva info disponible → leyendo configuración completa…");

    // ------------------------------
    // AHORA sí parseamos la config completa
    // ------------------------------
    parseConfigJSON(payload, true);

    // ------------------------------
    // GUARDAR EN NVS
    // ------------------------------
    guardarConfigEnPrefs();

    Serial.println("✔ Configuración actualizada y guardada en memoria.");

    onConfigUpdated();

    http.end();
}


void parseConfigJSON(const String &jsonStr, bool is_backend) {

    StaticJsonDocument<4096> doc;

    DeserializationError err = deserializeJson(doc, jsonStr);
    if (err) {
        Serial.print("❌ Error JSON: ");
        Serial.println(err.f_str());
        return;
    }
    
     String  id_guardado = doc["id_device"].as<String>();

    if (id_guardado.length() == 0 && !is_backend) {
        Serial.println("❌ No hay id_device en prefs");
        return;
    }

    if (id_guardado != id_device && !is_backend) {
        Serial.printf("Id_guardado: %d, Id_device:%d",id_guardado,id_device);
        Serial.println("❌ ID no coincide");
        return;
    }

    // ====== ROOT ======
    config.status = doc["status"] | 0;

    // ====== EQUIPO ======
    config.equipo.cantidad_valvulas = doc["equipo"]["cantidad_valvulas"] | 0;
    config.equipo.cantidad_sensores = doc["equipo"]["cantidad_sensores"] | 0;

    // ====== VALVULAS ======
    config.valvulas.clear();

    JsonArray arr = doc["valvulas"].as<JsonArray>();

    for (JsonVariant item : arr) {

        JsonObject v = item.as<JsonObject>();
        ValvulaConfig val;

        val.id_valvula      = v["id_valvula"] | 0;
        val.is_active       = v["is_active"].as<bool>();
        val.weather_enable  = v["weather_enable"].as<bool>();
        val.schedule_enable = v["schedule_enable"].as<bool>();
        val.sensor_enable   = v["sensor_enable"].as<bool>();
        val.manual_watering = v["manual_watering"].as<bool>();


        val.humidity_min    = v["humidity_min"] | 0;
        val.humidity_max    = v["humidity_max"] | 0;

        val.modo_control = v["modo_control"].as<String>();

        // ====== SCHEDULE ======
        JsonObject s = v["schedule"];

        val.schedule.modo_frecuencia = s["modo_frecuencia"].as<String>();
        val.schedule.interval_days   = s["interval_days"] | 0;
        val.schedule.duration        = s["duration"] | 0;

        // ===== DAYS OF WEEK =====
        JsonObject d = s["days_of_week"];

        val.schedule.days_of_week.monday_on    = d["monday_on"] | 0;
        val.schedule.days_of_week.tuesday_on   = d["tuesday_on"] | 0;
        val.schedule.days_of_week.wednesday_on = d["wednesday_on"] | 0;
        val.schedule.days_of_week.thursday_on  = d["thursday_on"] | 0;
        val.schedule.days_of_week.friday_on    = d["friday_on"] | 0;
        val.schedule.days_of_week.saturday_on  = d["saturday_on"] | 0;
        val.schedule.days_of_week.sunday_on    = d["sunday_on"] | 0;

        // ===== WATER TIMES =====
        val.schedule.water_times.clear();

        JsonArray times = s["water_times"].as<JsonArray>();
        for (JsonVariant t : times) {
            val.schedule.water_times.push_back(t.as<String>());
        }

        config.valvulas.push_back(val);
    }

    Serial.println("✅ Configuración cargada correctamente desde JSON");

    for (int i = 0; i < config.valvulas.size(); i++) {
    Serial.printf(
        "VALVULA %d → id=%d is_active=%d manual=%d modo=%s schedule_enable=%d sensor_enable=%d modo_frecuencia=%s interval_days=%d \n",
        i,
        config.valvulas[i].id_valvula,
        config.valvulas[i].is_active,
        config.valvulas[i].manual_watering,
        config.valvulas[i].modo_control.c_str(),
        config.valvulas[i].schedule_enable,
        config.valvulas[i].sensor_enable,
        config.valvulas[i].schedule.modo_frecuencia.c_str(),
        config.valvulas[i].schedule.interval_days
    );
    Serial.print("  water_times: ");
    if (config.valvulas[i].schedule.water_times.size() == 0) {
        Serial.println("(vacío)");
    } else {
        for (int j = 0; j < config.valvulas[i].schedule.water_times.size(); j++) {
            Serial.print(config.valvulas[i].schedule.water_times[j]);
            if (j < config.valvulas[i].schedule.water_times.size() - 1) {
                Serial.print(", ");
            }
        }
        Serial.println();
    }
    }
    

}

void reportWateringStatus(int id_valvula, bool is_watering) {
    HTTPClient http;

    String url = ip_server + "/wateringStatus";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    // Construimos el body EXACTO que espera el backend
    String body = "{";
    body += "\"id_equipo\":\"" + id_device + "\",";
    body += "\"id_valvula\":" + String(id_valvula) + ",";
    body += "\"is_watering\":" + String(is_watering ? 1 : 0);
    body += "}";

    int httpCode = http.PUT(body);

    if (httpCode <= 0) {
        Serial.printf("❌ Error mandando wateringStatus: %s\n",
                      http.errorToString(httpCode).c_str());
    } else {
        Serial.printf("🌐 wateringStatus enviado → válvula %d = %d\n",
                      id_valvula, is_watering);
    }

    http.end();
}

void reportHumidityLog(int id_valvula, float humedad, const struct tm &now) {
    HTTPClient http;

    String url = ip_server + "/addRegistroHumedad";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    char horaStr[20];
    // formato: YYYY-MM-DD HH:MM:SS
    sprintf(horaStr, "%04d-%02d-%02d %02d:%02d:%02d",
            now.tm_year + 1900,
            now.tm_mon + 1,
            now.tm_mday,
            now.tm_hour,
            now.tm_min,
            now.tm_sec);

    String body = "{";
    body += "\"id_equipo\":\"" + id_device + "\",";
    body += "\"id_valvula\":" + String(id_valvula) + ",";
    body += "\"humedad\":" + String(humedad, 1) + ",";
    body += "\"hora\":\"" + String(horaStr) + "\"";
    body += "}";

    int httpCode = http.POST(body);

    if (httpCode <= 0) {
        Serial.printf("❌ Error enviando humedad válvula %d: %s\n",
                      id_valvula,
                      http.errorToString(httpCode).c_str());
    } else {
        Serial.printf("📡 Humedad enviada válvula %d → %.1f%%\n",
                      id_valvula, humedad);
    }

    http.end();
}

void reportRiego(
    int id_valvula,
    float humedad,
    bool se_rego,
    bool lluvia,
    const struct tm &now
) {
    HTTPClient http;

    String url = ip_server + "/addRiego";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    char dateStr[11]; // YYYY-MM-DD
    char timeStr[6];  // HH:MM:SS

    sprintf(dateStr, "%04d-%02d-%02d",
            now.tm_year + 1900,
            now.tm_mon + 1,
            now.tm_mday);

    sprintf(timeStr, "%02d:%02d",
            now.tm_hour,
            now.tm_min);

    String body = "{";
    body += "\"id_valvula\":" + String(id_valvula) + ",";
    body += "\"id_equipo\":\"" + id_device + "\",";
    body += "\"lluvia\":" + String(lluvia ? 1 : 0) + ",";
    body += "\"date\":\"" + String(dateStr) + "\",";
    body += "\"time\":\"" + String(timeStr) + "\",";
    body += "\"se_rego\":" + String(se_rego ? 1 : 0) + ",";
    body += "\"humedad\":" + String(humedad, 1);
    body += "}";

    int httpCode = http.POST(body);

    if (httpCode <= 0) {
        Serial.printf("❌ Error reportando riego válvula %d: %s\n",
                      id_valvula,
                      http.errorToString(httpCode).c_str());
    } else {
        Serial.printf(
            "💧 Riego reportado válvula %d → %s %s (humedad %.1f%%)\n",
            id_valvula,
            dateStr,
            timeStr,
            humedad
        );
    }

    http.end();
}

void getWeatherForecast() {

    HTTPClient http;

    String url = ip_server + "/apiClima?id_device=" + id_device;
    Serial.println("[HTTP] GET clima → " + url);

    http.begin(url);
    http.setConnectTimeout(15000);
    http.setTimeout(20000);

    int httpCode = http.GET();

    if (httpCode <= 0) {
        Serial.printf("❌ Error HTTP clima: %s\n",
                      http.errorToString(httpCode).c_str());
        http.end();
        return;
    }

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("❌ Código HTTP clima inesperado: %d\n", httpCode);
        http.end();
        return;
    }

    String payload = http.getString();
    Serial.println("RAW clima payload:");
    Serial.println(payload);

    parseWeatherJSON(payload);

    lastWeatherUpdate = millis();

    http.end();
}

void parseWeatherJSON(const String &jsonStr) {

    StaticJsonDocument<2048> doc;

    DeserializationError err = deserializeJson(doc, jsonStr);
    if (err) {
        Serial.print("❌ Error JSON clima: ");
        Serial.println(err.f_str());
        return;
    }

    int status = doc["status"] | -1;
    if (status != 200) {
        Serial.println("❌ Status clima inválido");
        return;
    }

    JsonArray data = doc["data"].as<JsonArray>();

    climaHorasCount = 0;

    for (JsonVariant item : data) {
        if (climaHorasCount >= MAX_CLIMA_HORAS) break;

        climaHoras[climaHorasCount].time =
            item["time"].as<String>();

        climaHoras[climaHorasCount].rain_probability =
            item["rain_probability"] | 0;

        climaHorasCount++;
    }

    Serial.println("🌦 Clima horario cargado:");
    for (int i = 0; i < climaHorasCount; i++) {
        Serial.printf("  [%d] %s → %d%%\n",
                      i,
                      climaHoras[i].time.c_str(),
                      climaHoras[i].rain_probability);
    }
}

