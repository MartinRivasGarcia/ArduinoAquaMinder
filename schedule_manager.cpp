#include <Arduino.h>
#include "globals.h"
#include "hardware_map.h"
#include "pedidosHTTP.h"

// ============================================================================
//  SCHEDULE MANAGER
//  - Modo SELECTED_DAYS: riego en días específicos a horarios definidos
//  - Modo INTERVAL_DAYS: riego cada X días a un horario definido
// ============================================================================

// Cada válvula guardará el último día del año donde se ejecutó un riego.
// Evita riegos múltiples en el mismo día/minuto.
static long lastWateringMinute[16];     // soporta hasta 16 válvulas
static time_t nextIntervalTimestamp[16]; // timestamp del próximo riego del modo intervalo

void onConfigUpdated() {
    for (int i = 0; i < 3; i++) {
        nextIntervalTimestamp[i] = 0;
    }
}

// ============================================================================
// Obtener struct tm actual
// ============================================================================
bool getNow(struct tm &now) {
    if (!obtenerTiempo(now)) {
        Serial.println("⚠ No se pudo obtener hora actual");
        return false;
    }
    return true;
}

// ============================================================================
// Convierte DaysOfWeek → array[7] usando convención tm_wday (0 = domingo)
// ============================================================================
void buildDaysArray(bool outDays[7], const DaysOfWeek &dw) {
    outDays[0] = dw.sunday_on;
    outDays[1] = dw.monday_on;
    outDays[2] = dw.tuesday_on;
    outDays[3] = dw.wednesday_on;
    outDays[4] = dw.thursday_on;
    outDays[5] = dw.friday_on;
    outDays[6] = dw.saturday_on;
}

// ============================================================================
// PARSER "HH:MM" → horas y minutos
// ============================================================================
void parseTimeHM(const String &txt, int &h, int &m) {
    h = txt.substring(0,2).toInt();
    m = txt.substring(3,5).toInt();
}

// ============================================================================
// Devuelve true si hoy es uno de los días permitidos (SELECTED_DAYS)
// ============================================================================
bool isSelectedDay(const ValvulaConfig &conf, const struct tm &now) {
    bool dias[7];
    buildDaysArray(dias, conf.schedule.days_of_week);
    return dias[now.tm_wday];
}

// ============================================================================
// SELECTED_DAYS → retorna true si HOY y AHORA coincide con algún horario
// ============================================================================
bool shouldStartSelectedDays(int i) {
    const auto &conf = config.valvulas[i];
    struct tm now;
    if (!getNow(now)){
    Serial.println("Sali por el 1er if");
      return false;  
    } 

    if (!isSelectedDay(conf, now)){
        Serial.println("Sali por el 2do if");
     return false;
    }

    int currentHM = now.tm_hour * 60 + now.tm_min;

    for (auto &t : conf.schedule.water_times) {
        int h, m;
        parseTimeHM(t, h, m);
        int targetHM = h * 60 + m;

        if (currentHM == targetHM) {

            // timestamp actual en minutos (día + hora + minuto)
            long nowMinute =
                (now.tm_yday * 24L * 60L) +
                (now.tm_hour * 60L) +
                now.tm_min;

            // evitar repetir el MISMO riego
            if (lastWateringMinute[i] != nowMinute) {
                return true;
            }
        }

    }
    return false;
}

// ============================================================================
// INTERVAL_DAYS → calcular próximo timestamp después de ejecutar
// ============================================================================
time_t makeTimestamp(int y, int m, int d, int h, int min) {
    struct tm t = {0};
    t.tm_year = y - 1900;
    t.tm_mon  = m - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min  = min;
    t.tm_sec  = 0;
    return mktime(&t);
}

void scheduleNextInterval(int i) {
    auto &conf = config.valvulas[i];

    struct tm now;
    if (!getNow(now)) return;

    int h = 0, m = 0;
    if (conf.schedule.water_times.size() > 0)
        parseTimeHM(conf.schedule.water_times[0], h, m);

    int Y = now.tm_year + 1900;
    int M = now.tm_mon + 1;
    int D = now.tm_mday;

    time_t todayTarget = makeTimestamp(Y, M, D, h, m);
    time_t nowTS = time(nullptr);

    if (todayTarget + 60 > nowTS) {
        // todavía no pasó hoy → usar hoy
        nextIntervalTimestamp[i] = todayTarget;
    } else {
        // ya pasó → sumar interval_days
        nextIntervalTimestamp[i] =
            todayTarget + conf.schedule.interval_days * 86400;
    }

    Serial.printf(
        "📆 Próximo riego INTERVAL válvula %d → %ld\n",
        i, nextIntervalTimestamp[i]
    );
}


// ============================================================================
// INTERVAL_DAYS → ¿Debo regar ahora?
// ============================================================================
bool shouldStartIntervalDays(int i) {
    const auto &conf = config.valvulas[i];

    if (conf.schedule.interval_days <= 0){
        Serial.println("No anduve por interval_days");
        return false;
    } 
    if (conf.schedule.water_times.size() == 0) {
        Serial.println("No anduve por water_times vacio");
        return false;
    }

    time_t nowTS = time(nullptr);

    // inicializar si aún no existe
    if (nextIntervalTimestamp[i] == 0) {
        scheduleNextInterval(i);
    }

     // obtener minuto actual
    struct tm now;
    if (!getNow(now)) return false;

    long nowMinute =
        (now.tm_yday * 24L * 60L) +
        (now.tm_hour * 60L) +
        now.tm_min;

    // ⛔ ya regó en este minuto
    if (lastWateringMinute[i] == nowMinute) {
        return false;
    }

    if (nowTS >= nextIntervalTimestamp[i] &&
        nowTS <  nextIntervalTimestamp[i] + 60) {  // ventana 1 minuto

        // recalcular siguiente riego luego
        scheduleNextInterval(i);
        return true;
    }

    return false;
}

// ============================================================================
// Llama cuando ya se debe iniciar el riego
// ============================================================================
void startScheduledWatering(int i) {
    auto &rt   = runtime[i];
    auto &conf = config.valvulas[i];

    Serial.printf("💧 Iniciando riego programado en válvula %d\n", i);
    

    rt.isCurrentlyWatering = true;
    rt.wateringStart = millis();

    if (i < VALVE_PINS_COUNT) digitalWrite(VALVE_PINS[i], HIGH);
    reportWateringStatus(conf.id_valvula, true);

    notificacion("RIEGO INICIADO ZONA " + String(i + 1));

    struct tm now;
    if (getNow(now)) {
        lastWateringMinute[i] = (now.tm_yday * 24L * 60L) + (now.tm_hour * 60L) + now.tm_min;

         // ====== REPORTAR RIEGO ======
        float humedad = rt.lastHumidity; // si no hay sensor, puede ser 0
        reportRiego(
            conf.id_valvula,
            humedad,
            true,
            false,
            now
        );
    }
}

// ============================================================================
// Llama cuando ya debe terminar
// ============================================================================
void stopScheduledWatering(int i) {
    auto &rt   = runtime[i];
    auto &conf = config.valvulas[i];

    Serial.printf("⛔ Finalizando riego programado en válvula %d\n", i);

    rt.isCurrentlyWatering = false;

    if (i < VALVE_PINS_COUNT) digitalWrite(VALVE_PINS[i], LOW);
    reportWateringStatus(conf.id_valvula, false);
}


// ============================================================================
// Este método se llama desde valve_manager → updateValveScheduled()
// ============================================================================
void scheduleManagerUpdate(int i) {
    auto &conf = config.valvulas[i];
    auto &rt   = runtime[i];

    static bool initialized = false;
    if (!initialized) {
        for (int j = 0; j < 16; j++) {
            lastWateringMinute[j] = -1;
            nextIntervalTimestamp[j] = 0;
        }
        initialized = true;
    }

    // si no hay schedule → nada
    if (!conf.schedule_enable) return;

    // MODO SELECTED_DAYS ------------------------------------------------------
    if (conf.schedule.modo_frecuencia == "SELECTED_DAYS") {
        
        // Si no está regando: ver si debe iniciar
        if (!rt.isCurrentlyWatering) {
            if (shouldStartSelectedDays(i)) {
                startScheduledWatering(i);
                return;
            }
        }
    }

    // MODO INTERVAL_DAYS ------------------------------------------------------
    else if (conf.schedule.modo_frecuencia == "INTERVAL_DAYS") {

        if (!rt.isCurrentlyWatering) {
            if (shouldStartIntervalDays(i)) {
                startScheduledWatering(i);
                return;
            }
        }
    }

    // SI ESTÁ REGANDO → controlar la duración
    if (rt.isCurrentlyWatering) {
        unsigned long elapsed = millis() - rt.wateringStart;
        if (elapsed >= conf.schedule.duration * 1000UL) {
            stopScheduledWatering(i);
        }
    }
}
