// ================================================
// smartroom_node1.ino -- Salle A
// ESP32 #1 -- Edge Computing (Mosquitto local)
// ================================================
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>
#include <MHZ19.h>
#include <FastLED.h>

// ----- CONFIGURATION RESEAU -----
#define WIFI_SSID    "VotreNomWiFi"
#define WIFI_PASS    "VotreMotDePasse"
#define MQTT_SERVER  "192.168.1.100"   // IP Raspberry Pi / PC
#define MQTT_PORT    1883
#define NODE_ID      "smartroom-node-1"

// ----- BROCHES -----
#define DHT_PIN      4
#define DHT_TYPE     DHT22
#define PIR_PIN      34
#define RELAY1_PIN   25
#define RELAY2_PIN   26
#define BUZZER_PIN   23
#define LED_PIN      32
#define NUM_LEDS     10
#define MHZ19_RX     16
#define MHZ19_TX     17

// ----- TOPICS MQTT -----
#define TOPIC_DATA   "smartroom/" NODE_ID "/data"
#define TOPIC_CMD    "smartroom/cmds/" NODE_ID
#define TOPIC_STATUS "smartroom/" NODE_ID "/status"

// ----- OBJETS -----
DHT              dht(DHT_PIN, DHT_TYPE);
BH1750           bh1750;
MHZ19            mhz19;
HardwareSerial   co2Serial(1);
CRGB             leds[NUM_LEDS];
WiFiClient       wifiClient;
PubSubClient     mqtt(wifiClient);
SemaphoreHandle_t i2cMutex;

// ----- CALLBACK COMMANDES -----
void onCommand(char* topic, byte* payload, unsigned int length) {
    StaticJsonDocument<128> cmd;
    if (deserializeJson(cmd, payload, length)) return;

    const char* act = cmd["act"];
    bool on = (strcmp(cmd["val"], "ON") == 0);

    if      (strcmp(act, "relay1") == 0) digitalWrite(RELAY1_PIN, on ? HIGH : LOW);
    else if (strcmp(act, "relay2") == 0) digitalWrite(RELAY2_PIN, on ? HIGH : LOW);
    else if (strcmp(act, "buzzer") == 0) digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
    else if (strcmp(act, "led")    == 0) {
        fill_solid(leds, NUM_LEDS, on ? CRGB::White : CRGB::Black);
        FastLED.show();
    }
    Serial.printf("CMD: %s -> %s\n", act, on ? "ON" : "OFF");
}

// ----- CONNEXION -----
void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500); Serial.print(".");
    }
    Serial.println("\nWiFi OK");
}

void connectMQTT() {
    mqtt.setServer(MQTT_SERVER, MQTT_PORT);
    mqtt.setCallback(onCommand);
    while (!mqtt.connected()) {
        if (mqtt.connect(NODE_ID, TOPIC_STATUS, 1, true, "offline")) {
            mqtt.publish(TOPIC_STATUS, "online", true);
            mqtt.subscribe(TOPIC_CMD);
            Serial.println("MQTT OK");
        } else {
            Serial.printf("MQTT err=%d\n", mqtt.state());
            delay(5000);
        }
    }
}

// ----- ACQUISITION -----
void publishData() {
    StaticJsonDocument<256> doc;
    doc["id"]   = NODE_ID;
    doc["ts"]   = millis() / 1000;
    doc["temp"] = dht.readTemperature();
    doc["hum"]  = dht.readHumidity();
    doc["pres"] = digitalRead(PIR_PIN);
    doc["rssi"] = WiFi.RSSI();

    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(500))) {
        doc["lux"] = bh1750.readLightLevel();
        xSemaphoreGive(i2cMutex);
    }
    if (millis() > 180000) doc["co2"] = mhz19.getCO2();

    char buf[256];
    serializeJson(doc, buf);
    mqtt.publish(TOPIC_DATA, buf);
    Serial.println(buf);
}

// ----- TACHES FREERTOS -----
void taskMQTT(void* pv) {
    for (;;) {
        if (!mqtt.connected()) connectMQTT();
        mqtt.loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void taskAcquisition(void* pv) {
    for (;;) {
        publishData();
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

// ----- SETUP -----
void setup() {
    Serial.begin(115200);
    dht.begin();
    Wire.begin(21, 22);
    bh1750.begin();
    co2Serial.begin(9600, SERIAL_8N1, MHZ19_RX, MHZ19_TX);
    mhz19.begin(co2Serial);
    mhz19.autoCalibration(false);

    pinMode(PIR_PIN,    INPUT);
    pinMode(RELAY1_PIN, OUTPUT);
    pinMode(RELAY2_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.clear(); FastLED.show();

    i2cMutex = xSemaphoreCreateMutex();
    connectWiFi();
    connectMQTT();

    xTaskCreatePinnedToCore(taskMQTT,        "MQTT", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(taskAcquisition, "ACQ",  4096, NULL, 1, NULL, 1);
}

void loop() {}