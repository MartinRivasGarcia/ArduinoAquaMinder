//ProbandoProyecto.ino
#include <SPI.h>
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "ble_manager.h"
#include "pedidosHTTP.h"
#include "globals.h"
#include "valve_manager.h"

#define USE_SERIAL Serial

/**
  En el adc tenemos:
  Ch0, ch1 y ch3 los sensores
  Ch4, ch5 y ch6 las corrientes
*/

// Pin CS que elegimos
#define PIN_CS 23
#define MOSFET1 27
#define MOSFET2 26
#define MOSFET3 25

#define SENSOR1 0
#define SENSOR2 1
#define SENSOR3 3

#define CURRENT1 4
#define CURRENT2 5
#define CURRENT3 6

#define RSHUNT 0.47     // Resistencia de medición (ohms)
#define VREF   3.3       // Tensión de referencia del MCP3008 (V)

unsigned long lastConfigMillis = 0;
const unsigned long CONFIG_INTERVAL_MS = 2000; // 2 segundos

/*
  SI el dispositivo no tiene wifi o no lo encuentra, habilitar el BT para que el usuario le pase la red y pass por ahi
*/
void conectarWiFi(String ssid, String pass) {
    Serial.print("Conectando a ");
    Serial.println(ssid);

    WiFi.begin(ssid, pass);

    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 5) {
        delay(500);
        Serial.print(".");
        intentos++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi conectado");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nERROR: No se pudo conectar a WiFi");
        BLE_init();
    }
}

//Sincronizar el RTC con Internet
void sincronizarNTP() {
    const char* ntpServer = "pool.ntp.org";
    const long gmtOffset_sec = -3 * 3600;   // Argentina (UTC-3)
    const int daylightOffset_sec = 0;       // No DST en Argentina

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    Serial.println("Sincronizando hora con NTP...");

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("ERROR: No se pudo obtener hora desde NTP");
        return;
    }

    Serial.println("Hora sincronizada correctamente");
    Serial.print("Fecha: ");
    Serial.println(asctime(&timeinfo));
}

time_t obtenerEpoch() {
    return time(nullptr);   // devuelve segundos desde 1970
}


void setup() {
  Serial.begin(115200);


  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }
  cargarWifi();
  conectarWiFi(wifi_ssid,wifi_pass);
  
  sincronizarNTP();

  // Inicializa SPI en pines por defecto (VSPI)
  SPI.begin(18, 19, 22, PIN_CS);

  pinMode(MOSFET1, OUTPUT);
  digitalWrite(MOSFET1, 0);

  // Inicializa MCP3008
  if (!adc.begin(PIN_CS)) {
    Serial.println("Error: no se encontró MCP3008");
    while (1);
  }
  Serial.println("MCP3008 listo para leer.");

  getConfig();
  initValves();
  notificacion("El equipo se conecto a la Red ");
  for (int i = 0; i < config.equipo.cantidad_valvulas; i++) {
        updateValve(i);
        if (!runtime[i].isCurrentlyWatering) {
            reportWateringStatus(config.valvulas[i].id_valvula, false);
        }
  }
}

void loop() {
    unsigned long nowMillis = millis();
    struct tm ahora;

    // actualizar clima cada 1 hora
 /*   if (millis() - lastWeatherUpdate > 3600000UL || climaHorasCount == 0) {
        getWeatherForecast();
    }*/

    // --- Pedir configuración cada 3 segundos ---
    if (nowMillis - lastConfigMillis >= CONFIG_INTERVAL_MS) {
        lastConfigMillis = nowMillis;
        obtenerTiempo(ahora);
        Serial.print("Hora actual: ");
        Serial.print(ahora.tm_hour); Serial.print(":"); Serial.println(ahora.tm_min);
        
        getConfig();
    }

    // --- Actualizar válvulas ---
    for (int i = 0; i < config.equipo.cantidad_valvulas; i++) {
        updateValve(i);
    }
}

/*

  int valorCH0 = adc.readADC(SENSOR1);  // Lee canal 0
  int valorCH1 = adc.readADC(SENSOR2);  // Lee canal 1
  int valorCH2 = adc.readADC(2);  // Lee canal 2
  int valorCH3 = adc.readADC(3);  // Lee canal 3
  int valorCH4 = adc.readADC(4);  // Lee canal 4

 Serial.print("CH0: ");
  Serial.print(valorCH0);
  Serial.print("\tCH1: ");
  Serial.println(valorCH1);
  Serial.print("\tCH2: ");
  Serial.println(valorCH2);
  Serial.print("\tCH3: ");
  Serial.println(valorCH3);
  Serial.print("\tCH4: ");
  Serial.println(valorCH4);

  // Calcula voltaje sobre la resistencia shunt
  float voltajeShunt = (valorCH4 * VREF) / 1023.0;

  // Calcula corriente
  float corriente = voltajeShunt / RSHUNT;
  Serial.print(" V -> Tension: ");
  Serial.print(voltajeShunt, 3);
  Serial.print(" I -> Corriente: ");
  Serial.print(corriente, 3);
  Serial.println(" A");
*/
