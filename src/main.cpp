#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ESP32Servo.h>

// LED pins
#define LED1 5
#define LED2 18
#define LED3 19

// Servo pin
#define SERVO_PIN 32

#define SSID "pussyslayer47"
#define PASSWORD "28021981"

#define MQTT_SERVER "172.20.10.13"
#define MQTT_PORT 1883

// Sparkplug B configuration
#define SPB_GROUP_ID "Ignition"
#define SPB_EDGE_NODE_ID "Master"
#define SPB_DEVICE_ID "Ventilation"

#define DHTPIN 4     // DHT sensor pin
#define DHTTYPE DHT22   // DHT 22

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

// Servo object
Servo myServo;

// Tag variables - from Ignition commands
float tag_sp1 = 22.0;
float tag_sp2 = 23.0;
float tag_sp3 = 24.0;
float tag_eco_sp = 21.0;
int tag_mode = 0;

// Fan control outputs - from Ignition commands
bool tag_fan1_state = false;
bool tag_fan2_state = false;
bool tag_fan3_state = false;

int tag_fan1_speed = 0;
int tag_fan2_speed = 0;
int tag_fan3_speed = 0;

// Local sensor readings
float tag_temp = 25.0;

// Sparkplug B sequence number
uint64_t seq = 0;
uint64_t bdSeq = 0;

// Placeholder for actual ventilator control
void spinVentilators(bool fan1, bool fan2, bool fan3, int speed1, int speed2, int speed3) {
    // TODO: Implement PWM or relay control
    Serial.printf("Fan1: %d (speed %d), Fan2: %d (speed %d), Fan3: %d (speed %d)\n", 
                  fan1, speed1, fan2, speed2, fan3, speed3);
}

// Parse Sparkplug B metrics from NDATA/NCMD/DCMD
void parseSparkplugMetrics(JsonDocument& doc) {
    JsonArray metrics = doc["metrics"];
    
    for (JsonObject metric : metrics) {
        const char* name = metric["name"];
        
        // Check for sp1 (fan speed control)
        if (strcmp(name, "sp1") == 0) {
            tag_sp1 = metric["value"];
            Serial.printf("✓ Updated SP1: %.2f\n", tag_sp1);
        }
        else if (strcmp(name, "sp2") == 0) {
            tag_sp2 = metric["value"];
            Serial.printf("✓ Updated SP2: %.2f\n", tag_sp2);
        }
        else if (strcmp(name, "sp3") == 0) {
            tag_sp3 = metric["value"];
            Serial.printf("✓ Updated SP3: %.2f\n", tag_sp3);
        }
        else if (strcmp(name, "eco_sp") == 0) {
            tag_eco_sp = metric["value"];
            Serial.printf("✓ Updated Eco SP: %.2f\n", tag_eco_sp);
        }
        else if (strcmp(name, "mode") == 0) {
            tag_mode = metric["value"];
            Serial.printf("✓ Updated Mode: %d\n", tag_mode);
        }
        else if (strcmp(name, "fan1_speed") == 0) {
            tag_fan1_speed = metric["value"];
            Serial.printf("✓ Updated Fan1 Speed: %d\n", tag_fan1_speed);
        }
        else if (strcmp(name, "fan1_state") == 0) {
            tag_fan1_state = metric["value"];
            Serial.printf("✓ Updated Fan1 State: %d\n", tag_fan1_state);
        }
        else if (strcmp(name, "fan2_state") == 0) {
            tag_fan2_state = metric["value"];
            Serial.printf("✓ Updated Fan2 State: %d\n", tag_fan2_state);
        }
        else if (strcmp(name, "fan3_state") == 0) {
            tag_fan3_state = metric["value"];
            Serial.printf("✓ Updated Fan3 State: %d\n", tag_fan3_state);
        }
        else if (strcmp(name, "fan2_speed") == 0) {
            tag_fan2_speed = metric["value"];
            Serial.printf("✓ Updated Fan2 Speed: %d\n", tag_fan2_speed);
        }
        else if (strcmp(name, "fan3_speed") == 0) {
            tag_fan3_speed = metric["value"];
            Serial.printf("✓ Updated Fan3 Speed: %d\n", tag_fan3_speed);
        }
    }
    
    spinVentilators(tag_fan1_state, tag_fan2_state, tag_fan3_state,
                    tag_fan1_speed, tag_fan2_speed, tag_fan3_speed);
}

// MQTT Callback - receives Sparkplug B messages from Ignition
void callback(char* topic, byte* payload, unsigned int length) {
    Serial.println("\n========== MQTT MESSAGE RECEIVED ==========");
    Serial.print("Topic: ");
    Serial.println(topic);
    Serial.print("Length: ");
    Serial.println(length);
    Serial.println("==========================================\n");

    // Build topic strings for comparison
    char ndataTopic[128];
    snprintf(ndataTopic, sizeof(ndataTopic), "spBv1.0/%s/NDATA/%s", 
             SPB_GROUP_ID, SPB_EDGE_NODE_ID);
    
    char ncmdTopic[128];
    snprintf(ncmdTopic, sizeof(ncmdTopic), "spBv1.0/%s/NCMD/%s", 
             SPB_GROUP_ID, SPB_EDGE_NODE_ID);
    
    char dcmdTopic[128];
    snprintf(dcmdTopic, sizeof(dcmdTopic), "spBv1.0/%s/DCMD/%s/%s", 
             SPB_GROUP_ID, SPB_EDGE_NODE_ID, SPB_DEVICE_ID);
    
    // Check if this is a NDATA, NCMD, or DCMD message
    if (strcmp(topic, ndataTopic) == 0 || strcmp(topic, ncmdTopic) == 0 || strcmp(topic, dcmdTopic) == 0) {
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        
        if (error) {
            Serial.print("✗ JSON parsing failed: ");
            Serial.println(error.c_str());
            return;
        }
        
        if (strcmp(topic, ndataTopic) == 0) {
            Serial.println("✓ Received Sparkplug B NDATA");
        } else {
            Serial.println("✓ Received Sparkplug B Command");
        }
        
        parseSparkplugMetrics(doc);
    }
}

void setup_wifi() {
    delay(10);
    Serial.println("\n\nStarting WiFi connection...");
    Serial.print("Connecting to: ");
    Serial.println(SSID);

    WiFi.begin(SSID, PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    Serial.println("");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Failed to connect to WiFi");
    }
}

// Send Sparkplug B DBIRTH (Device Birth) message
void sendDBIRTH() {
    StaticJsonDocument<2048> doc;
    
    unsigned long timestamp = millis();
    doc["timestamp"] = timestamp;
    doc["seq"] = seq++;
    
    JsonArray metrics = doc.createNestedArray("metrics");
    
    // Add all device metrics with initial values
    JsonObject temp = metrics.createNestedObject();
    temp["name"] = "temp";
    temp["timestamp"] = timestamp;
    temp["dataType"] = "Float";
    temp["value"] = tag_temp;
    
    JsonObject mode = metrics.createNestedObject();
    mode["name"] = "mode";
    mode["timestamp"] = timestamp;
    mode["dataType"] = "Int32";
    mode["value"] = tag_mode;
    
    JsonObject sp1 = metrics.createNestedObject();
    sp1["name"] = "sp1";
    sp1["timestamp"] = timestamp;
    sp1["dataType"] = "Float";
    sp1["value"] = tag_sp1;
    
    JsonObject sp2 = metrics.createNestedObject();
    sp2["name"] = "sp2";
    sp2["timestamp"] = timestamp;
    sp2["dataType"] = "Float";
    sp2["value"] = tag_sp2;
    
    JsonObject sp3 = metrics.createNestedObject();
    sp3["name"] = "sp3";
    sp3["timestamp"] = timestamp;
    sp3["dataType"] = "Float";
    sp3["value"] = tag_sp3;
    
    JsonObject eco_sp = metrics.createNestedObject();
    eco_sp["name"] = "eco_sp";
    eco_sp["timestamp"] = timestamp;
    eco_sp["dataType"] = "Float";
    eco_sp["value"] = tag_eco_sp;
    
    // Fan states and speeds
    JsonObject fan1_state = metrics.createNestedObject();
    fan1_state["name"] = "fan1_state";
    fan1_state["timestamp"] = timestamp;
    fan1_state["dataType"] = "Boolean";
    fan1_state["value"] = tag_fan1_state;
    
    JsonObject fan1_speed = metrics.createNestedObject();
    fan1_speed["name"] = "fan1_speed";
    fan1_speed["timestamp"] = timestamp;
    fan1_speed["dataType"] = "Int32";
    fan1_speed["value"] = tag_fan1_speed;
    
    JsonObject fan2_state = metrics.createNestedObject();
    fan2_state["name"] = "fan2_state";
    fan2_state["timestamp"] = timestamp;
    fan2_state["dataType"] = "Boolean";
    fan2_state["value"] = tag_fan2_state;
    
    JsonObject fan2_speed = metrics.createNestedObject();
    fan2_speed["name"] = "fan2_speed";
    fan2_speed["timestamp"] = timestamp;
    fan2_speed["dataType"] = "Int32";
    fan2_speed["value"] = tag_fan2_speed;
    
    JsonObject fan3_state = metrics.createNestedObject();
    fan3_state["name"] = "fan3_state";
    fan3_state["timestamp"] = timestamp;
    fan3_state["dataType"] = "Boolean";
    fan3_state["value"] = tag_fan3_state;
    
    JsonObject fan3_speed = metrics.createNestedObject();
    fan3_speed["name"] = "fan3_speed";
    fan3_speed["timestamp"] = timestamp;
    fan3_speed["dataType"] = "Int32";
    fan3_speed["value"] = tag_fan3_speed;
    
    char buffer[2048];
    size_t n = serializeJson(doc, buffer);
    
    char topic[128];
    snprintf(topic, sizeof(topic), "spBv1.0/%s/DBIRTH/%s/%s", 
             SPB_GROUP_ID, SPB_EDGE_NODE_ID, SPB_DEVICE_ID);
    
    client.publish(topic, buffer, n);
    Serial.println("✓ Published DBIRTH message");
}

// Send Sparkplug B DDATA (Device Data) message
void sendDDATA() {
    StaticJsonDocument<1024> doc;
    
    unsigned long timestamp = millis();
    doc["timestamp"] = timestamp;
    doc["seq"] = seq++;
    
    JsonArray metrics = doc.createNestedArray("metrics");
    
    // Only send changed values (or all values periodically)
    JsonObject temp = metrics.createNestedObject();
    temp["name"] = "temp";
    temp["timestamp"] = timestamp;
    temp["dataType"] = "Float";
    temp["value"] = tag_temp;
    
    JsonObject mode = metrics.createNestedObject();
    mode["name"] = "mode";
    mode["timestamp"] = timestamp;
    mode["dataType"] = "Int32";
    mode["value"] = tag_mode;
    
    char buffer[1024];
    size_t n = serializeJson(doc, buffer);
    
    char topic[128];
    snprintf(topic, sizeof(topic), "spBv1.0/%s/DDATA/%s/%s", 
             SPB_GROUP_ID, SPB_EDGE_NODE_ID, SPB_DEVICE_ID);
    
    client.publish(topic, buffer, n);
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Connecting to MQTT broker...");
        if (client.connect("ESP32_Ventilation", "mlting47", "28021981")) {
            Serial.println("connected!");
            
            // Subscribe to NDATA (Node Data) - Ignition publishes data here
            char ndataTopic[128];
            snprintf(ndataTopic, sizeof(ndataTopic), "spBv1.0/%s/NDATA/%s", 
                     SPB_GROUP_ID, SPB_EDGE_NODE_ID);
            client.subscribe(ndataTopic);
            
            // Subscribe to NCMD (Node Command) - for commands
            char ncmdTopic[128];
            snprintf(ncmdTopic, sizeof(ncmdTopic), "spBv1.0/%s/NCMD/%s", 
                     SPB_GROUP_ID, SPB_EDGE_NODE_ID);
            client.subscribe(ncmdTopic);
            
            // Also subscribe to DCMD for device-specific commands
            char dcmdTopic[128];
            snprintf(dcmdTopic, sizeof(dcmdTopic), "spBv1.0/%s/DCMD/%s/%s", 
                     SPB_GROUP_ID, SPB_EDGE_NODE_ID, SPB_DEVICE_ID);
            client.subscribe(dcmdTopic);
            
            Serial.println("✓ Subscribed to Sparkplug B topics:");
            Serial.println(ndataTopic);
            Serial.println(ncmdTopic);
            Serial.println(dcmdTopic);
            
            // Send DBIRTH after connection
            delay(100);
            sendDBIRTH();
            
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" - retrying in 5 seconds");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(LED3, OUTPUT);
    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);
    digitalWrite(LED3, LOW);

    // Initialize servo
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    myServo.setPeriodHertz(50);
    myServo.attach(SERVO_PIN, 1000, 2000);

    dht.begin();
    setup_wifi();
    
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(callback);
}

// Averaging buffers
#define AVG_WINDOW 50
float tempBuffer[AVG_WINDOW] = {0};
float sp1Buffer[AVG_WINDOW] = {0};
int avgIndex = 0;
int avgCount = 0;

// Read temperature from DHT or simulate
float readTemperature() {
    static float temp = 19.5f;
    static unsigned long lastUpdate = 0;
    static float phase = 0.0f;
    unsigned long now = millis();

    if (now - lastUpdate > 1000) {
        lastUpdate = now;
        phase += 0.15f + ((float)random(-10, 10) / 100.0f);
        if (phase > 6.28319f) phase -= 6.28319f;
        float base = 19.5f + 2.0f * sin(phase);

        temp += ((float)random(-10, 11)) / 100.0f;
        temp += (base - temp) * 0.1f;

        if (temp < 17.0f) temp = 17.0f;
        if (temp > 22.0f) temp = 22.0f;
    }
    return temp;
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    static unsigned long lastSend = 0;
    static unsigned long lastServoUpdate = 0;
    static int servoAngle = 0;
    static int servoDirection = 1;
    unsigned long now = millis();
    
    if (now - lastSend > 5000) {
        lastSend = now;

        // Read current temperature
        tag_temp = readTemperature();

        // Averaging logic
        tempBuffer[avgIndex] = tag_temp;
        sp1Buffer[avgIndex] = tag_sp1;
        avgIndex = (avgIndex + 1) % AVG_WINDOW;
        if (avgCount < AVG_WINDOW) avgCount++;

        float sumTemp = 0.0f, sumSp1 = 0.0f;
        for (int i = 0; i < avgCount; i++) {
            sumTemp += tempBuffer[i];
            sumSp1 += sp1Buffer[i];
        }
        float avgTemp = sumTemp / avgCount;
        float avgSp1 = sumSp1 / avgCount;

        // Publish telemetry to MQTT (simple format)
        StaticJsonDocument<512> doc;
        doc["temp"] = tag_temp;
        doc["avgTemp"] = avgTemp;
        doc["sp1"] = tag_sp1;
        doc["avgSp1"] = avgSp1;
        doc["sp2"] = tag_sp2;
        doc["sp3"] = tag_sp3;
        doc["eco_sp"] = tag_eco_sp;
        doc["mode"] = tag_mode;
        doc["fan1_state"] = tag_fan1_state;
        doc["fan2_state"] = tag_fan2_state;
        doc["fan3_state"] = tag_fan3_state;
        doc["fan1_speed"] = tag_fan1_speed;
        doc["fan2_speed"] = tag_fan2_speed;
        doc["fan3_speed"] = tag_fan3_speed;

        char jsonBuffer[512];
        size_t n = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
        client.publish("ventilation", jsonBuffer, n);

        Serial.println("Published telemetry to MQTT");
    }

    // Servo control based on mode and sp1
    if (myServo.attached()) {
        // Mode controls servo on/off (0 = off, 1 = on)
        if (tag_mode == 0) {
            // Servo off - keep it at 0 degrees
            myServo.write(0);
        } else {
            // Servo on - speed controlled by sp1
            // Clamp sp1 to [10, 40]
            float sp1_clamped = tag_sp1;
            if (sp1_clamped < 10) sp1_clamped = 10;
            if (sp1_clamped > 40) sp1_clamped = 40;

            // Map sp1: 10 (slowest) -> 50ms, 40 (fastest) -> 2ms
            int stepDelay = map((int)sp1_clamped, 10, 40, 50, 2);

            if (now - lastServoUpdate > stepDelay) {
                lastServoUpdate = now;
                myServo.write(servoAngle);
                servoAngle += servoDirection;

                // Reverse direction at limits
                if (servoAngle >= 180) {
                    servoAngle = 180;
                    servoDirection = -1;
                } else if (servoAngle <= 0) {
                    servoAngle = 0;
                    servoDirection = 1;
                }
            }
        }
    }
}