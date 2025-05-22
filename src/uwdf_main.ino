/*
 * uwdf_main.ino - Main program for UWDF Platform on ESP32S2
 * Copyright (C) 2025 UWDF Team
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <WiFi.h>
#include <WebSocketServer.h>
#include <ArduinoJson.h>
#include <XLOT_MAX301xx_PulseOximeter.h>
#include <Adafruit_BME280.h>
#include <vector>
#include <string>

// Configuration constants
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const int SERVER_PORT = 80;
const int NUM_SENSORS = 4; // Added SpO2 sensor
const int NUM_READINGS = 10; // Number of readings for averaging
const float FILTER_WEIGHT = 0.5; // Weight for exponential moving average
const int REPORTING_PERIOD_MS = 500; // Reporting interval
const int JSON_DOC_SIZE_SMALL = 200; // JSON document size for single sensor
const int JSON_DOC_SIZE_LARGE = 1024; // JSON document size for multiple sensors
const int CLIENT_ID_ADMIN = 1;
const int CLIENT_ID_GUEST = 2;
const int MIN_HEART_RATE = 30; // Minimum valid heart rate (bpm)
const int MAX_HEART_RATE = 220; // Maximum valid heart rate (bpm)
const int MIN_SPO2 = 50; // Minimum valid SpO2 (%)
const int BEAT_TIMEOUT_MS = 1000; // Timeout for beat detection

// WebSocket server configuration
WiFiServer server(SERVER_PORT);
WebSocketServer webSocket;

// Sensor instances
PulseOximeter pulseOximeter;
Adafruit_BME280 bme;

// Sensor configuration
struct Sensor {
    int id;
    String name;
    float (*readFunction)();
};

// Sensor data structure
struct SensorData {
    int sensorId;
    float value;
    String timestamp;
    bool isValid;
};

// Sensor read functions
float readHeartRate() {
    pulseOximeter.update();
    return pulseOximeter.getHeartRate();
}

float readSpO2() {
    pulseOximeter.update();
    return pulseOximeter.getSpO2();
}

float readTemperature() {
    float temp = bme.readTemperature();
    if (isnan(temp)) {
        return 0.0; // Return 0.0 if reading fails
    }
    return temp; // Temperature in °C
}

float readMotion() {
    // Placeholder: Replace with actual motion sensor read (e.g., using I2C)
    return random(0, 100); // Motion intensity
}

Sensor sensors[NUM_SENSORS] = {
    {1, "heart_rate", readHeartRate},
    {2, "spo2", readSpO2},
    {3, "temperature", readTemperature},
    {4, "motion", readMotion}
};

// Client permission structure
struct ClientPermission {
    int clientId;
    std::vector<String> allowedSensors;
};

// Global variables
std::vector<ClientPermission> clientPermissions;
SensorData lastSentData[NUM_SENSORS];
uint32_t tsLastReport = 0;
uint32_t lastBeat = 0;
int readIndex = 0;
float averageHeartRate = 0;
float averageSpO2 = 0;
bool calculationComplete = false;
bool calculating = false;
bool initialized = false;

// Calculate averaged sensor data
SensorData calculateSensorData(int sensorId, const String& sensorName, float rawValue) {
    static SensorData currentData = {sensorId, 0.0, String(millis()), false};
    
    if (sensorName == "heart_rate" || sensorName == "spo2") {
        float heartRate = sensorName == "heart_rate" ? rawValue : readHeartRate();
        float spO2 = sensorName == "spo2" ? rawValue : readSpO2();
        
        if (readIndex == NUM_READINGS) {
            calculationComplete = true;
            calculating = false;
            initialized = false;
            readIndex = 0;
            currentData.value = sensorName == "heart_rate" ? averageHeartRate : averageSpO2;
            currentData.isValid = true;
        }
        
        if (!calculationComplete && heartRate > MIN_HEART_RATE && heartRate < MAX_HEART_RATE && spO2 > MIN_SPO2) {
            if (sensorName == "heart_rate") {
                averageHeartRate = FILTER_WEIGHT * heartRate + (1 - FILTER_WEIGHT) * averageHeartRate;
            } else {
                averageSpO2 = FILTER_WEIGHT * spO2 + (1 - FILTER_WEIGHT) * averageSpO2;
            }
            readIndex++;
            currentData.isValid = false;
        }
        
        if (millis() - lastBeat > BEAT_TIMEOUT_MS) {
            calculationComplete = false;
            averageHeartRate = 0;
            averageSpO2 = 0;
            currentData.isValid = false;
        }
    } else {
        // Non-MAX30100 sensors (temperature, motion)
        static float previousValues[NUM_READINGS] = {0};
        static int index = 0;
        
        previousValues[index] = rawValue;
        index = (index + 1) % NUM_READINGS;
        
        float smoothedValue = 0;
        for (int i = 0; i < NUM_READINGS; i++) {
            smoothedValue += previousValues[i];
        }
        smoothedValue /= NUM_READINGS;
        
        currentData.value = smoothedValue;
        currentData.isValid = true;
    }
    
    currentData.sensorId = sensorId;
    currentData.timestamp = String(millis());
    return currentData;
}

// Callback for beat detection
void onBeatDetected() {
    lastBeat = millis();
}

// Access control mechanism
bool hasPermission(int clientId, const String& sensorType) {
    for (const auto& permission : clientPermissions) {
        if (permission.clientId == clientId) {
            return std::find(
                permission.allowedSensors.begin(),
                permission.allowedSensors.end(),
                sensorType) != permission.allowedSensors.end();
        }
    }
    return false;
}

// Serialize sensor data to JSON
String serializeSensorData(const SensorData& data) {
    StaticJsonDocument<JSON_DOC_SIZE_SMALL> doc;
    doc["sensorId"] = data.sensorId;
    doc["value"] = data.value;
    doc["timestamp"] = data.timestamp;
    
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

void setup() {
    Serial.begin(115200);
    
    // Initialize heart rate and SpO2 sensor
    if (!pulseOximeter.begin(PULSEOXIMETER_DEBUGGINGMODE_NONE)) {
        Serial.println("Failed to initialize MAX30100 sensor");
        while (true); // Halt if sensor initialization fails
    }
    pulseOximeter.setOnBeatDetectedCallback(onBeatDetected);
    
    // Initialize temperature sensor
    if (!bme.begin(0x76)) { // I2C address, typically 0x76 or 0x77
        Serial.println("Failed to initialize BME280 sensor");
        while (true); // Halt if sensor initialization fails
    }
    
    // Connect to Wi-Fi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    Serial.println(WiFi.localIP());
    
    // Start server
    server.begin();
    
    // Initialize client permissions
    clientPermissions.push_back({CLIENT_ID_ADMIN, {"heart_rate", "spo2", "temperature"}});
    clientPermissions.push_back({CLIENT_ID_GUEST, {"motion"}});
}

void loop() {
    WiFiClient client = server.available();
    
    // Data transfer logic
    if (client.connected() && webSocket.handshake(client)) {
        int clientId = CLIENT_ID_ADMIN; // Replace with authenticated client ID
        while (client.connected()) {
            bool dataChanged = false;
            StaticJsonDocument<JSON_DOC_SIZE_LARGE> doc;
            
            if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
                for (int i = 0; i < NUM_SENSORS; i++) {
                    if (hasPermission(clientId, sensors[i].name)) {
                        SensorData currentData = calculateSensorData(sensors[i].id, sensors[i].name, sensors[i].readFunction());
                        if (currentData.isValid && currentData.timestamp != lastSentData[i].timestamp) {
                            JsonObject sensorObj = doc.createNestedObject(sensors[i].name);
                            sensorObj["sensorId"] = currentData.sensorId;
                            sensorObj["value"] = currentData.value;
                            sensorObj["timestamp"] = currentData.timestamp;
                            lastSentData[i] = currentData;
                            dataChanged = true;
                        }
                    }
                }
                tsLastReport = millis();
            }
            
            // Send data only when changes occur
            if (dataChanged) {
                String jsonString;
                serializeJson(doc, jsonString);
                webSocket.sendData(jsonString);
            }
            
            delay(100); // Adjust as needed
        }
        Serial.println("Client disconnected");
    }
}