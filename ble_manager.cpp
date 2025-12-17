//ble_manager.cpp
#include "ble_manager.h"
#include <WiFi.h>
#include <Preferences.h>

// ===============================
//  Variables globales internas
// ===============================
const char* WIFI_SERVICE_UUID        = "0000abcd-0000-1000-8000-00805f9b34fb";
const char* WIFI_CHARACTERISTIC_UUID = "0000abce-0000-1000-8000-00805f9b34fb";

NimBLECharacteristic* wifiChar = nullptr;
NimBLEServer* server = nullptr;

static String lastSSID = "";
static String lastPASS = "";

// ===============================
//  Callbacks del servidor BLE
// ===============================
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        Serial.println("Client connected!");
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        Serial.println("Client disconnected!");
    }
};



// ===============================
//  Callback de escritura de SSID:PASS
// ===============================
class WifiCharCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
        
        std::string raw = pChar->getValue();
        if (raw.empty()) {
            Serial.println("Escritura vacía en characteristic");
            return;
        }

        String data = String(raw.c_str());
        Serial.println("=== BLE WRITE RECIBIDO ===");
        Serial.print("Datos crudos: ");
        Serial.println(data);

        int sep = data.indexOf(':');
        if (sep < 0) {
            Serial.println("Formato inválido. Se esperaba ssid:password");
            wifiChar->setValue("WIFI_FAIL");
            return;
        }

        lastSSID = data.substring(0, sep);
        lastPASS = data.substring(sep + 1);

        Serial.print("SSID recibido: ");
        Serial.println(lastSSID);

        Serial.print("PASS recibida: ");
        Serial.println(lastPASS);

        // Intentar conexión WiFi
        Serial.println("Intentando conectar WiFi...");
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(true);
        delay(100);

        WiFi.begin(lastSSID.c_str(), lastPASS.c_str());

        unsigned long start = millis();
        const unsigned long timeout = 15000;
        while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout) {
            delay(200);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi conectado!");
            guardarWifi(lastSSID.c_str(), lastPASS.c_str());
            Serial.print("IP: ");
            Serial.println(WiFi.localIP());

            wifiChar->setValue("WIFI_OK");
        } else {
            Serial.println("No se pudo conectar.");
            wifiChar->setValue("WIFI_FAIL");
            BLE_init();
        }

        Serial.println("===========================");
    }
};

// ===============================
//  Inicialización BLE
// ===============================
void BLE_init() {
    Serial.println("=== ESP32 NimBLE - Inicializando BLE ===");

    NimBLEDevice::init(DEVICE_NAME);
    NimBLEDevice::setSecurityAuth(false, false, false);

    server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    NimBLEService* service = server->createService(WIFI_SERVICE_UUID);

    wifiChar = service->createCharacteristic(
        WIFI_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ
    );

    wifiChar->setCallbacks(new WifiCharCallbacks());
    wifiChar->setValue("WAITING");

    service->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(WIFI_SERVICE_UUID);

    NimBLEAdvertisementData scanData;
    scanData.setName(DEVICE_NAME);
    adv->setScanResponseData(scanData);

    adv->start();

    Serial.println("BLE Advertising iniciado.");
    Serial.print("Service UUID: ");
    Serial.println(WIFI_SERVICE_UUID);
    Serial.print("Characteristic UUID: ");
    Serial.println(WIFI_CHARACTERISTIC_UUID);
    Serial.println("Esperando escritura de SSID:PASS...");
}


