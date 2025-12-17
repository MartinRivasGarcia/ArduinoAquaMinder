#include "valve_manager.h"
#include "hardware_map.h"
#include "schedule_manager.h"
#include <Arduino.h>

extern Adafruit_MCP3008 adc;
extern ConfigSistema config;
extern std::vector<ValveRuntime> runtime;

// Contador de pines (definir en hardware_map.h preferentemente)
#ifndef VALVE_PINS_COUNT
#define VALVE_PINS_COUNT 3
#endif

#define SENSOR_DRY   750   // aire / tierra muy seca
#define SENSOR_WET   260   // agua


// --------------------------------------------------
// Marca si el riego fue iniciado por modo MANUAL
// --------------------------------------------------
static std::vector<bool> manualOrigin; // tamaño = cantidad de válvulas, inicializado en initValves()

// =======================================================
//  INIT
// =======================================================
void initValves() {
    runtime.clear();
    runtime.resize(config.valvulas.size());

    // inicializo vector de manualOrigin
    manualOrigin.clear();
    manualOrigin.resize(config.valvulas.size(), false);

    for (size_t i = 0; i < config.valvulas.size(); i++) {
        if ((int)i < VALVE_PINS_COUNT) {
            pinMode(VALVE_PINS[i], OUTPUT);
            digitalWrite(VALVE_PINS[i], LOW);
        }

        runtime[i].isCurrentlyWatering = false;
        runtime[i].wateringStart = 0;
        runtime[i].lastWateringEpoch = 0;
        runtime[i].lastHumidityLogHour = -1;
    }

    Serial.println("✔ initValves(): listo (manualOrigin inicializado).");
}

// =======================================================
// UTILITARIOS
// =======================================================
// startValve ahora acepta indicador si fue por manual o no.
// Si byManual==true -> marca manualOrigin[i] = true
// Si byManual==false -> manualOrigin[i] = false (arranque por sensor)
void startValve(int i, bool byManual = false) {
    if (i < 0 || i >= (int)runtime.size()) return;

    if (i < VALVE_PINS_COUNT) digitalWrite(VALVE_PINS[i], HIGH);

    runtime[i].isCurrentlyWatering = true;
    runtime[i].wateringStart = millis();
    runtime[i].lastWateringEpoch = time(nullptr);

    manualOrigin[i] = byManual;

    reportWateringStatus(config.valvulas[i].id_valvula, true);
    notificacion("RIEGO INICIADO ZONA " + String(i + 1));

    Serial.printf("🟢 RIEGO INICIADO (válvula %d) origen: %s\n",
                  i, (byManual ? "MANUAL" : "OTRO"));

    // ====== REPORTAR RIEGO ======
    struct tm now;
    if (obtenerTiempo(now)) {
        // humedad estimada: si viene de sensor, usar última medida
        float humedad = runtime[i].lastHumidity; // si no existe, poner 0
        reportRiego(
            config.valvulas[i].id_valvula,
            humedad,
            true,   // se_rego
            false,  // lluvia (por ahora)
            now
        );
    }
}

// stopValve deja manualOrigin en false (porque ya no está regando)
void stopValve(int i) {
    if (i < 0 || i >= (int)runtime.size()) return;

    if (i < VALVE_PINS_COUNT) digitalWrite(VALVE_PINS[i], LOW);

    if (runtime[i].isCurrentlyWatering) {
        runtime[i].isCurrentlyWatering = false;
        reportWateringStatus(config.valvulas[i].id_valvula, false);
        Serial.printf("🔴 RIEGO DETENIDO (válvula %d)\n", i);
    }

    // limpiar flag de origen manual al cortar
    manualOrigin[i] = false;
}

// Función pública que otros módulos (p.e. schedule_manager) pueden llamar
// para forzar que la válvula no sea considerada iniciada por manual.
// Útil si schedule_manager inicia un riego directamente.
void clearManualOriginFlag(int i) {
    if (i < 0 || i >= (int)manualOrigin.size()) return;
    manualOrigin[i] = false;
}

// =======================================================
//  UPDATE
// =======================================================
void updateValve(int i) {

    if (i < 0 || i >= (int)config.valvulas.size()) return;

    auto &conf = config.valvulas[i];
    auto &rt   = runtime[i];

    // Desactivada por completo
    if (!conf.is_active) {
        stopValve(i);
        return;
    }

    // Modo manual: prioridad absoluta
    if (conf.manual_watering == 1) {
        // Si flag = 1 y no está regando → iniciar por manual
        if (!rt.isCurrentlyWatering) {
            startValve(i, true); // inicio por manual
            manualOrigin[i] = true;
        } else {
            // si ya está regando (por cualquier motivo) lo marcamos como manual-origin
            manualOrigin[i] = true;
        }
        return;
    }
    else {
        

        // Si el flag pasó a 0, SOLO APAGAMOS si el origen fue manual
        if (manualOrigin[i]) {
            // apagar porque fue originado por manual y user pidió apagar
            stopValve(i);
            manualOrigin[i] = false;
            Serial.printf("🔴 RIEGO DETENIDO por manual (válvula %d)\n", i);
            // manualOrigin limpiado por stopValve()
            // NO return; permitimos que sensores/schedule tomen el control luego
        }

      /*  if (conf.weather_enable) {
            if (climaHorasCount == 0) {
                Serial.println("⚠️ Clima habilitado pero sin datos → no bloqueo riego");
            }
            else if (shouldBlockIrrigationByWeather(70)) {
                Serial.println("🚫 Riego bloqueado por lluvia");
                return;
            }
        }*/

        // MODO SENSOR
        // MODO SCHEDULED (SelectedDays o IntervalDays)
        if (conf.schedule_enable) {
            // Nota: schedule_manager puede iniciar la válvula directamente.
            // Si schedule_manager inicia el riego, debería llamar clearManualOriginFlag(i)
            // para que no queden residuos del flag manual.
            scheduleManagerUpdate(i);
            return;
        }
        if (conf.sensor_enable) {
            updateValveSensor(i);
            return;
        }
    }
    
}

// =======================================================
// SENSOR DE HUMEDAD
// =======================================================
void updateValveSensor(int i) {
    if (i < 0 || i >= (int)config.valvulas.size()) return;

    auto &conf = config.valvulas[i];
    auto &rt   = runtime[i];

    int raw = adc.readADC(SENSOR_CHANNELS[i]);
    if (raw < 0) {
        Serial.printf("⚠ ADC inválido en sensor %d\n", i);
        return;
    }

    if(raw < 100){
        stopValve(i);
        return;
    }

    // Transformación lineal simplificada (calibrar en tu caso)
    float humedad = 100.0 * (SENSOR_DRY - raw) / (SENSOR_DRY - SENSOR_WET);
    humedad = constrain(humedad, 0.0, 100.0);

    saveHumedad(humedad, i);
    rt.lastHumidity = humedad;
   // Serial.printf("Sensor %d raw=%d humid=%0.1f\n", i, raw, humedad);

    if (!rt.isCurrentlyWatering) {
        if (humedad <= conf.humidity_min) {
            startValve(i, false); // inicio por sensor (byManual=false)
        }
        return;
    }

    if (humedad >= conf.humidity_max) {
        // Solo detener si no fue encendida por manual.
        if (manualOrigin[i]) {
            Serial.printf("🔒 No detengo válvula %d por humedad porque origen=MANUAL\n", i);
            // si querés forzar detención aún con origen manual, cambiar lógica aquí
        } else {
            stopValve(i);
        }
    }
}

void saveHumedad (int humedad, int i){
    // ================================
    // REGISTRO DE HUMEDAD (1 vez por hora)
    // ================================
    struct tm now;
    auto &conf = config.valvulas[i];
    auto &rt   = runtime[i];
    static int lastsecond = 0;
    
    if (obtenerTiempo(now)) {

        if (rt.lastHumidityLogHour != now.tm_hour) {

            Serial.printf(
                "📝 Log humedad válvula %d → %.1f%% (hora %02d)\n",
                i, humedad, now.tm_hour
            );

            reportHumidityLog(conf.id_valvula, humedad, now);

            // Guardar hora ya registrada
            rt.lastHumidityLogHour = now.tm_hour;
        }
    
    }

}

bool shouldBlockIrrigationByWeather(uint8_t threshold) {

    for (int i = 0; i < climaHorasCount; i++) {
        if (climaHoras[i].rain_probability >= threshold) {
            return true;
        }
    }
    return false;
}
