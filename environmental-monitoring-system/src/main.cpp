#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <MQUnifiedsensor.h>

// Pin Definitions
#define DHT_PIN 4
#define DHTTYPE DHT11
#define DUST_LED_PIN 12   // GP2Y1014AU0F LED pin
#define DUST_MEASURE_PIN 13  // GP2Y1014AU0F measure pin
#define MQ135_PIN_A0 34  // MQ135 analog input
#define MQ135_PIN_D0 35  // MQ135 digital input for threshold

// OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// GP2Y1014AU0F Dust Sensor Timing
#define samplingTime 280
#define deltaTime 40
#define sleepTime 9680

// MQTT Topics
#define TOPIC_STATE_TEMPERATURE1 "esp32/sensor/dht11/temperature"
#define TOPIC_STATE_HUMIDITY1 "esp32/sensor/dht11/humidity"
#define TOPIC_STATE_TEMPERATURE2 "esp32/sensor/aht20/temperature"
#define TOPIC_STATE_HUMIDITY2 "esp32/sensor/aht20/humidity"
#define TOPIC_STATE_PRESSURE "esp32/sensor/pressure"
#define TOPIC_STATE_DUST "esp32/sensor/dust"
#define TOPIC_STATE_AIR_QUALITY "esp32/sensor/air_quality"

// EEPROM Configuration
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 32
#define MQTT_IP_ADDR 64

// MQ135 Configuration
#define BOARD "ESP-32"
#define VOLTAGE_RESOLUTION 3.3
#define ADC_BIT_RESOLUTION 12
#define RatioMQ135CleanAir 3.6

// Device initialization
DHT dht(DHT_PIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
MQUnifiedsensor MQ135(BOARD, VOLTAGE_RESOLUTION, ADC_BIT_RESOLUTION, MQ135_PIN_A0, "MQ-135");

// WiFi and MQTT Configuration
char ssid[32] = "vanhoa";
char password[32] = "11111111";
char mqtt_server[32] = "192.168.4.2";
const int mqtt_port = 1883;
WiFiClient espClient;
PubSubClient client(espClient);

// Sensor Variables
float dht_temperature = 0, dht_humidity = 0;
float aht_temperature = 0, aht_humidity = 0;
float pressure = 0;
float dust_density = 0;
float air_quality = 0;

// Averaging variables
float tempSumDHT = 0, humiSumDHT = 0;
float tempSumAHT = 0, humiSumAHT = 0;
float pressureSum = 0;
float dustSum = 0;
float airQualitySum = 0;
int count = 0;
int countTime = 0;

float avgDHTTemp = 0, avgDHTHumi = 0;
float avgAHTTemp = 0, avgAHTHumi = 0;
float avgPressure = 0;
float avgDust = 0;
float avgAirQuality = 0;

// State variables
bool updateDisplay = false;
bool publishSensorFlag = false;
bool isAPMode = false;
bool bmpInitialized = false;
bool ahtInitialized = false;
bool oledInitialized = false;

// Task Handles
TaskHandle_t TaskReadHandle;
TaskHandle_t TaskPrintHandle;
TaskHandle_t TaskDisplayHandle;
TaskHandle_t TaskMQTTHandle;

// Function declarations
void loadCredentials();
void saveCredentials();
bool setup_wifi();
void handleRoot();
void handleConfig();
void startAPMode();

// Function to read dust sensor
float readDustSensor() {
    float voMeasured = 0;
    float calcVoltage = 0;
    float dustDensity = 0;
    
    // Đọc điện áp nền (khi LED tắt)
    digitalWrite(DUST_LED_PIN, HIGH);
    delayMicroseconds(sleepTime);
    float baseVoltage = analogRead(DUST_MEASURE_PIN);
    
    // Đọc giá trị khi LED sáng
    digitalWrite(DUST_LED_PIN, LOW);
    delayMicroseconds(samplingTime);
    voMeasured = analogRead(DUST_MEASURE_PIN);
    delayMicroseconds(deltaTime);
    digitalWrite(DUST_LED_PIN, HIGH);
    
    // Tính điện áp thực từ giá trị ADC (ESP32 ADC 12-bit)
    calcVoltage = (voMeasured - baseVoltage) * (3.3 / 4095.0);
    
    // Công thức chuyển đổi điện áp sang mật độ bụi (đã hiệu chỉnh cho ESP32)
    // Hệ số 0.17 được điều chỉnh thành 0.2 để phù hợp với điện áp thực tế
    if (calcVoltage > 0) {
        dustDensity = 0.2 * calcVoltage * 1000.0; // Chuyển đổi sang μg/m3
    } else {
        dustDensity = 0;
    }
    
    // Giới hạn giá trị hợp lý
    if (dustDensity < 0) {
        dustDensity = 0;
    } else if (dustDensity > 500) {  // Giới hạn trên thông thường của cảm biến
        dustDensity = 500;
    }
    
    return dustDensity;
}

void TaskReadSensor(void *pvParameters) {
    sensors_event_t humidity, temp;
    
    for(;;) {
        // Read DHT11
        float t1 = dht.readTemperature();
        float h1 = dht.readHumidity();
        if (!isnan(t1) && !isnan(h1)) {
            dht_temperature = t1;
            dht_humidity = h1;
            tempSumDHT += t1;
            humiSumDHT += h1;
        }
        
        // Read AHT20
        if (aht.getEvent(&humidity, &temp)) {
            aht_temperature = temp.temperature;
            aht_humidity = humidity.relative_humidity;
            tempSumAHT += aht_temperature;
            humiSumAHT += aht_humidity;
        }

        // Read BMP280
        float p = bmp.readPressure();
        if (p > 0 && p < 200000) {  // Valid pressure range is roughly 30000-110000 Pa
            p = p / 100.0; // Convert Pa to hPa
            pressure = p;
            pressureSum += p;
        } else {
            Serial.println("Invalid BMP280 pressure reading");
        }

        // Read MQ135
        MQ135.update();
        float aq = MQ135.readSensor();
        bool threshold_triggered = digitalRead(MQ135_PIN_D0);  // Read digital threshold pin
        
        if (!isnan(aq)) {
            air_quality = aq;
            airQualitySum += aq;
            if (threshold_triggered) {
                Serial.println("MQ135 threshold exceeded!");
            }
        }

        // Read GP2Y1014AU0F
        float dust = readDustSensor();
        dustSum += dust;
        dust_density = dust;

        count++;
        countTime++;

        Serial.println("\n=== Current Readings ===");
        Serial.printf("DHT11  - T: %.1f°C, H: %.1f%%\n", dht_temperature, dht_humidity);
        Serial.printf("AHT20  - T: %.1f°C, H: %.1f%%\n", aht_temperature, aht_humidity);
        Serial.printf("BMP280 - P: %.1f hPa\n", pressure);
        Serial.printf("MQ135  - AQ: %.1f ppm\n", air_quality);
        Serial.printf("Dust   - %.2f mg/m³\n", dust_density);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void TaskPrintData(void *pvParameters) {
    for(;;) {
        if (countTime >= 15) {  // Average over 30 seconds
            if (count > 0) {
                avgDHTTemp = tempSumDHT / count;
                avgDHTHumi = humiSumDHT / count;
                avgAHTTemp = tempSumAHT / count;
                avgAHTHumi = humiSumAHT / count;
                avgPressure = pressureSum / count;
                avgDust = dustSum / count;
                avgAirQuality = airQualitySum / count;

                Serial.println("\n=== 30-Second Averages ===");
                Serial.printf("DHT11 - T: %.1f°C, H: %.1f%%\n", avgDHTTemp, avgDHTHumi);
                Serial.printf("AHT20 - T: %.1f°C, H: %.1f%%\n", avgAHTTemp, avgAHTHumi);
                Serial.printf("Pressure: %.1f hPa\n", avgPressure);
                Serial.printf("Air Quality: %.1f ppm\n", avgAirQuality);
                Serial.printf("Dust: %.2f mg/m³\n", avgDust);

                updateDisplay = true;
                publishSensorFlag = true;

                // Reset accumulators
                tempSumDHT = humiSumDHT = 0;
                tempSumAHT = humiSumAHT = 0;
                pressureSum = dustSum = airQualitySum = 0;
                count = countTime = 0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void TaskDisplay(void *pvParameters) {
    for(;;) {
        if (updateDisplay) {
            display.clearDisplay();

            // Draw rectangles
            display.drawRect(0, 22, 127, 20, 1);
            display.drawRect(0, 1, 127, 19, 1);

            // Draw vertical line
            display.drawLine(62, 2, 62, 40, 1);

            // Configure text settings
            display.setTextColor(1);
            display.setTextWrap(false);

            // Draw text in each section
            char buf[20];
            
            // Top row - DHT11 and AHT20 labels
            display.setCursor(4, 7);
            display.print("DHT11");
            display.setCursor(66, 7);
            display.print("AHT20");

            // Middle row - Temperature values
            snprintf(buf, sizeof(buf), "%.1fC", avgDHTTemp);
            display.setCursor(4, 28);
            display.print(buf);

            snprintf(buf, sizeof(buf), "%.1fC", avgAHTTemp);
            display.setCursor(66, 28);
            display.print(buf);

            // Bottom row - Pressure
            snprintf(buf, sizeof(buf), "%.1f hPa", avgPressure);
            display.setCursor(3, 49);
            display.print(buf);

            display.display();
            updateDisplay = false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void publishSensorData() {
    if (!client.connected()) return;

    char msg[10];
    
    snprintf(msg, 10, "%.1f", avgDHTTemp);
    client.publish(TOPIC_STATE_TEMPERATURE1, msg);
    
    snprintf(msg, 10, "%.1f", avgDHTHumi);
    client.publish(TOPIC_STATE_HUMIDITY1, msg);
    
    snprintf(msg, 10, "%.1f", avgAHTTemp);
    client.publish(TOPIC_STATE_TEMPERATURE2, msg);
    
    snprintf(msg, 10, "%.1f", avgAHTHumi);
    client.publish(TOPIC_STATE_HUMIDITY2, msg);
    
    snprintf(msg, 10, "%.1f", avgPressure);
    client.publish(TOPIC_STATE_PRESSURE, msg);
    
    snprintf(msg, 10, "%.2f", avgDust);
    client.publish(TOPIC_STATE_DUST, msg);
    
    snprintf(msg, 10, "%.1f", avgAirQuality);
    client.publish(TOPIC_STATE_AIR_QUALITY, msg);
}

void TaskMQTT(void *pvParameters) {
    for(;;) {
        if (!isAPMode && publishSensorFlag) {
            publishSensorData();
            publishSensorFlag = false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// WiFi and configuration functions
void loadCredentials() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(SSID_ADDR, ssid);
    EEPROM.get(PASS_ADDR, password);
    EEPROM.get(MQTT_IP_ADDR, mqtt_server);
    EEPROM.end();
}

void saveCredentials() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(SSID_ADDR, ssid);
    EEPROM.put(PASS_ADDR, password);
    EEPROM.put(MQTT_IP_ADDR, mqtt_server);
    EEPROM.commit();
    EEPROM.end();
}

bool setup_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("\nConnected to WiFi. IP: ");
        Serial.println(WiFi.localIP());
        return true;
    }
    return false;
}

void handleRoot() {
    String html = "<html><body>";
    html += "<h1>ESP32 Environmental Monitor Setup</h1>";
    html += "<form method='post' action='/config'>";
    html += "SSID: <input type='text' name='ssid'><br>";
    html += "Password: <input type='password' name='pwd'><br>";
    html += "MQTT Server: <input type='text' name='mqtt'><br>";
    html += "<input type='submit' value='Save'>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
}

void handleConfig() {
    String newSsid = server.arg("ssid");
    String newPass = server.arg("pwd");
    String newMqtt = server.arg("mqtt");
    
    if (newSsid.length() > 0 && newPass.length() > 0 && newMqtt.length() > 0) {
        strncpy(ssid, newSsid.c_str(), 31);
        strncpy(password, newPass.c_str(), 31);
        strncpy(mqtt_server, newMqtt.c_str(), 31);
        saveCredentials();
        server.send(200, "text/plain", "Settings saved. Restarting...");
        delay(2000);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Invalid input!");
    }
}

void startAPMode() {
    isAPMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32_EnvMonitor", "12345678");
    
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    
    server.on("/", handleRoot);
    server.on("/config", HTTP_POST, handleConfig);
    server.begin();
    
    display.clearDisplay();
    display.setTextColor(1);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("AP Mode Active");
    display.println("SSID: ESP32_EnvMonitor");
    display.println("Pass: 12345678");
    display.println(WiFi.softAPIP().toString());
    display.display();
}

void setup() {
    Serial.begin(115200);
    Wire.begin();

    // Initialize dust sensor
    pinMode(DUST_LED_PIN, OUTPUT);
    digitalWrite(DUST_LED_PIN, HIGH);

    // Initialize MQ135
    pinMode(MQ135_PIN_D0, INPUT);  // Digital pin for threshold detection
    MQ135.init();
    MQ135.setRegressionMethod(1);
    MQ135.setA(110.47);
    MQ135.setB(-2.862);
    float calcR0 = 0;
    for(int i = 1; i <= 10; i++) {
        MQ135.update();
        calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
        delay(100);
    }
    MQ135.setR0(calcR0/10);

    // Initialize DHT11
    dht.begin();
    
    // Initialize AHT20
    if (!aht.begin()) {
        Serial.println("Could not find AHT20");
    }

    // Initialize BMP280
    int attempts = 0;
    while (!bmp.begin(0x77) && attempts < 3) {
        Serial.println("Could not find BMP280! Retrying...");
        attempts++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    bmpInitialized = bmp.begin(0x77);
    
    if (bmpInitialized) {
        Serial.println("BMP280 init success!");
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                       Adafruit_BMP280::SAMPLING_X2,
                       Adafruit_BMP280::SAMPLING_X16,
                       Adafruit_BMP280::FILTER_X16,
                       Adafruit_BMP280::STANDBY_MS_500);
        vTaskDelay(pdMS_TO_TICKS(100));
    } else {
        Serial.println("BMP280 init failed!");
    }

    // Initialize OLED
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(1);
    display.setCursor(0,0);
    display.println("Initializing...");
    display.display();

    // Load credentials and setup WiFi
    loadCredentials();
    if (!setup_wifi()) {
        startAPMode();
    } else {
        client.setServer(mqtt_server, mqtt_port);
    }

    // Create tasks
    xTaskCreatePinnedToCore(TaskReadSensor, "ReadSensor", 4096, NULL, 3, &TaskReadHandle, 1);
    xTaskCreatePinnedToCore(TaskPrintData, "PrintData", 4096, NULL, 1, &TaskPrintHandle, 1);
    xTaskCreatePinnedToCore(TaskDisplay, "Display", 4096, NULL, 1, &TaskDisplayHandle, 0);
    xTaskCreatePinnedToCore(TaskMQTT, "MQTT", 4096, NULL, 1, &TaskMQTTHandle, 1);
}

void loop() {
    if (isAPMode) {
        server.handleClient();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}
