#ifndef RUNTIME_TYPES_H
#define RUNTIME_TYPES_H

#include <Arduino.h>
#include <vector>
#include <time.h>

struct ValveRuntime {
    unsigned long wateringStart= 0;

    // Últimos 10 valores del sensor
    int lastHumiditySamples[10];
    int sampleIndex;

    // Control interno del modo INTERVAL_DAYS
    unsigned long lastWateringDayTimestamp;
    bool isCurrentlyWatering = false;
    time_t lastWateringEpoch = 0;
    time_t lastHumidityLogHour;
    float lastHumidity;

};

#endif
