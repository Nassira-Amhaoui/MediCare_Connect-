#include <WiFi.h>
#include <DHT.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"

// Pin and sensor definitions
#define DHTPIN 18        // Pin connected to DHT11
#define DHTTYPE DHT11   // DHT sensor type

DHT dht(DHTPIN, DHTTYPE);

const int PulseSensorPurplePin = 32;  // Pulse sensor pin
int Signal;                           // Raw data from pulse sensor
const int Threshold = 2048;           // Heartbeat detection threshold
unsigned long lastBeatTime = 0;       // Time of last heartbeat
int BPM = 0;                          // Beats per minute

// MAX30100 configuration
PulseOximeter pox;
uint32_t tsLastReport = 0;

// Network information
IPAddress ip(192, 168, 137, 247);
IPAddress gateway(192, 168, 43, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 43, 1);

const char* ssid = "andr";
const char* password = "123456789";

WiFiServer server(80);

void onBeatDetected() {
    Serial.println("Beat detected!");
}

void setup() {
    Serial.begin(115200);
    dht.begin();  // Initialize DHT sensor

    // Initialize MAX30100 sensor
    if (!pox.begin()) {
        Serial.println("ERROR: Failed to initialize pulse oximeter");
    } else {
        Serial.println("Pulse oximeter initialized");
    }
    pox.setOnBeatDetectedCallback(onBeatDetected);

    delay(10);

    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.config(ip, dns, gateway, subnet);
    WiFi.begin(ssid, password);

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected.");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());

        server.begin();
    } else {
        Serial.println("Failed to connect to WiFi.");
    }
}

void loop() {
    // Update MAX30100 sensor
    pox.update();

    // Read DHT11 sensor data
    float temperature = dht.readTemperature(true);
    float humidity = dht.readHumidity();

    // Check if readings are valid
    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    // Read pulse sensor data
    Signal = analogRead(PulseSensorPurplePin);
    unsigned long currentTime = millis();

    // Heartbeat detection logic for pulse sensor
    if (Signal > Threshold) {
        if (currentTime - lastBeatTime > 600) { // Avoid false positives (600 ms between beats)
            unsigned long interval = currentTime - lastBeatTime;
            lastBeatTime = currentTime;

            BPM = 60000 / interval; // Calculate BPM
            Serial.print("BPM: ");
            Serial.println(BPM);
            Serial.print("Temperature: ");
            Serial.println(temperature);
            Serial.print("BPM (MAX30100): ");
            Serial.println(pox.getHeartRate());
            Serial.print("SpO2 (MAX30100): ");
            Serial.println(pox.getSpO2());
        }
    }

    // Handle client connections
    handleClient(temperature, humidity);
    delay(100); // Delay for analog read
}

void handleClient(float temperature, float humidity) {
    WiFiClient client = server.available();
    if (client) {
        Serial.println("New Client.");
        while (client.connected() && !client.available()) {
            delay(1);
        }

        if (client.available()) {
            String request = client.readStringUntil('\r');
            Serial.println(request);
            client.flush();

            // Send HTTP response
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/plain");
            client.println();

            // Send single line response
            client.print("BPM:");
            client.print(BPM);
            client.print("/Temp:");
            client.print(temperature);
            client.print("/Humidity:");
            client.print(humidity);
            client.print("BPM(MAX30100):");
            client.print(pox.getHeartRate());
            client.print("/SpO2(MAX30100):");
            client.print(pox.getSpO2());
        }

        client.stop();
        Serial.println("Client disconnected.");
    }
    delay(20);
}
