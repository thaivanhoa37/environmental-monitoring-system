/***********************************************************************************
 * IoT Environmental Monitoring System
 * 
 * This system monitors environmental conditions using:
 * - Temperature & Humidity sensor (AHT20)
 * - Pressure sensor (BMP280)
 * - CO2 sensor (MQ135)
 * - OLED Display
 * - WiFi connectivity
 * - MQTT communication
 * 
 * Hardware Connections:
 * MQ135 -> ESP32
 * - VCC  -> 5V
 * - GND  -> GND  
 * - AOUT -> GPIO35 (ADC1_CH7)
 * 
 * Lưu ý: Cảm biến MQ135 cần nguồn 5V để hoạt động chính xác
 * 
 * Note: Using ADC1 channel to avoid conflicts with WiFi
 * ADC2 channels cannot be used when WiFi is active
 * 
 * Main Features:
 * 1. Sensor data collection and averaging
 * 2. Web configuration interface
 * 3. MQTT integration
 * 4. OLED status display
 ***********************************************************************************/
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// MQTT Topic Definitions
const char* TOPIC_STATE_TEMPERATURE = "esp32/sensor/temperature";
const char* TOPIC_STATE_HUMIDITY = "esp32/sensor/humidity";
const char* TOPIC_STATE_PRESSURE = "esp32/sensor/pressure";
const char* TOPIC_STATE_CO2 = "esp32/sensor/co2";

// OLED Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Cấu hình MQ135
#define MQ135_PIN 35     // GPIO35 (ADC1_CH7)
#define RZERO 76.63      // Điện trở của cảm biến trong không khí sạch (kΩ)
#define PARA 116.6020682 // Hệ số hiệu chỉnh theo nhiệt độ/độ ẩm
#define PPM_MIN 400      // CO2 trong không khí sạch
#define PPM_MAX 5000     // CO2 tối đa có thể đo

// Sensor initialization
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
bool bmpInitialized = false;
bool ahtInitialized = false;
bool oledInitialized = false;

// EEPROM Configuration
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 32
#define MQTT_IP_ADDR 64

// Server initialization
AsyncWebServer server(80);

// WiFi and MQTT Configuration
char ssid[32] = "vanhoa";
char password[32] = "11111111";
char mqtt_server[32] = "192.168.137.127";
const int mqtt_port = 1883;
const char* mqtt_client_id = "ESP32_IOT_Monitor";

// State variables
float temperature = 0, humidity = 0, pressure = 0, co2ppm = 0;
float tempSum = 0, humiSum = 0, pressSum = 0, co2Sum = 0;
int count = 0;
int countTime = 0;

float avgTemperature = 0;
float avgHumidity = 0;
float avgPressure = 0;
float avgCO2 = 0;
bool updateDisplay = false;
bool publishSensorFlag = false;
bool isAPMode = false;

char msg[50];

// Connection management variables
int failedReconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 10;
const unsigned long RESET_TIMEOUT = 300000;  // 5 minutes

WiFiClient espClient;
PubSubClient client(espClient);

// Task handles
TaskHandle_t TaskReadHandle;
TaskHandle_t TaskPrintHandle;
TaskHandle_t TaskDisplayHandle;
TaskHandle_t TaskMQTTHandle;

// Function declarations
void TaskReadSensor(void *pvParameters);
void TaskPrintData(void *pvParameters);
void TaskDisplay(void *pvParameters);
void TaskMQTT(void *pvParameters);
bool setup_wifi();
void loadCredentials();
void saveCredentials(const char* newSsid, const char* newPass, const char* newMqtt);
void startAPMode();
bool reconnect();
void publishMQTTSensor();
float readCO2();

/*
 * Function: Read CO2 from MQ135
 * Returns CO2 concentration in ppm
 */
float readCO2() {
    // Đọc giá trị ADC
    int adc = analogRead(MQ135_PIN);
    float voltage = adc * (3.3 / 4095.0); // ESP32 ADC = 3.3V
    
    // Tính điện trở của cảm biến (Rs)
    float rs = ((5.0 * 20.0) / voltage) - 20.0; // 20kΩ = điện trở phân áp
    
    // Tính tỉ lệ Rs/Ro
    float ratio = rs / RZERO;
    
    // Tính PPM dựa trên công thức: ppm = PARA * (Rs/Ro)^-1.41
    float ppm = PARA * pow(ratio, -1.41);
    
    // Giới hạn giá trị PPM
    ppm = constrain(ppm, PPM_MIN, PPM_MAX);
    
    // In thông tin debug
    static unsigned long lastDebugPrint = 0;
    if (millis() - lastDebugPrint > 2000) {  // In mỗi 2 giây
        Serial.println("\n----------------------------------------");
        Serial.printf("  MQ135 Sensor Readings:\n");
        Serial.printf("  ADC Value : %d (0-4095)\n", adc);
        Serial.printf("  Voltage   : %.2fV (0-3.3V)\n", voltage);
        Serial.printf("  Rs Value  : %.1f kΩ\n", rs);
        Serial.printf("  Rs/Ro     : %.3f\n", ratio);
        Serial.printf("  CO2 Level : %.0f ppm\n", ppm);
        Serial.println("----------------------------------------\n");
        lastDebugPrint = millis();
    }
    
    // Temperature compensation
    if(temperature > 20) {
        float t_factor = 1.0 + 0.018 * (temperature - 20.0);  // 1.8% per °C
        ppm = ppm * t_factor;
    }
    
    // Humidity compensation (above 65%)
    if(humidity > 65) {
        float h_factor = 1.0 + 0.015 * (humidity - 65.0);  // 1.5% per %RH
        ppm = ppm * h_factor;
    }
    
    // Final range check
    ppm = constrain(ppm, PPM_MIN, PPM_MAX);
    return ppm;
}

String getCO2Quality(float ppm) {
  if (ppm < 800) return "tot";  // Good air quality
  if (ppm < 1200) return "TB";  // Average
  if (ppm < 2000) return "kem"; // Poor
  return "Nguy!";               // Dangerous
}

void scanI2C() {
  Serial.println("Scanning I2C...");
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("Found I2C device at address: 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  Serial.println("I2C scan complete.");
}

bool initializeOLED() {
  Serial.print("Initializing OLED at address: 0x");
  Serial.println(SCREEN_ADDRESS, HEX);
  if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED initialized successfully!");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("OLED Initialized");
    display.display();
    return true;
  }
  Serial.println("❌ Failed to initialize OLED! Check I2C connection.");
  return false;
}

void loadCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(SSID_ADDR, ssid);
  EEPROM.get(PASS_ADDR, password);
  EEPROM.get(MQTT_IP_ADDR, mqtt_server);
  EEPROM.end();
  Serial.println("Loaded credentials from EEPROM:");
  Serial.print("SSID: "); Serial.println(ssid);
  Serial.print("MQTT Server: "); Serial.println(mqtt_server);
}

void saveCredentials(const char* newSsid, const char* newPass, const char* newMqtt) {
  char tempSsid[32], tempPass[32], tempMqtt[32];
  strncpy(tempSsid, newSsid, 32);
  strncpy(tempPass, newPass, 32);
  strncpy(tempMqtt, newMqtt, 32);
  tempSsid[31] = tempPass[31] = tempMqtt[31] = '\0';

  if (strcmp(tempSsid, ssid) == 0 && strcmp(tempPass, password) == 0 && strcmp(tempMqtt, mqtt_server) == 0) {
    Serial.println("No changes in credentials, skipping EEPROM write");
    return;
  }

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(SSID_ADDR, tempSsid);
  EEPROM.put(PASS_ADDR, tempPass);
  EEPROM.put(MQTT_IP_ADDR, tempMqtt);
  EEPROM.commit();
  EEPROM.end();

  strncpy(ssid, tempSsid, 32);
  strncpy(password, tempPass, 32);
  strncpy(mqtt_server, tempMqtt, 32);
  Serial.println("New credentials saved to EEPROM");
}

bool setup_wifi() {
  WiFi.mode(WIFI_STA);
  Serial.printf("Connecting to %s\n", ssid);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    failedReconnectAttempts = 0;
    return true;
  }
  Serial.println("\nWiFi connection failed");
  return false;
}

String getConfigPage() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="UTF-8">
      <title>ESP32 Configuration</title>
      <style>
        body { font-family: Arial, sans-serif; background-color: #f4f4f4; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
        .container { background-color: #fff; padding: 30px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); width: 100%; max-width: 400px; }
        h1 { text-align: center; color: #333; }
        input[type="text"], input[type="password"] { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 5px; }
        input[type="submit"] { background-color: #28a745; color: white; padding: 10px 15px; border: none; border-radius: 5px; width: 100%; cursor: pointer; }
        input[type="submit"]:hover { background-color: #218838; }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>ESP32 Configuration</h1>
        <form method='POST' action='/save'>
          WiFi SSID:<br>
          <input type='text' name='ssid' value='%SSID%'><br>
          Password:<br>
          <input type='password' name='pass' value='%PASS%'><br>
          MQTT Server:<br>
          <input type='text' name='mqtt' value='%MQTT%'><br>
          <input type='submit' value='Save'>
        </form>
      </div>
    </body>
    </html>
  )rawliteral";

  html.replace("%SSID%", String(ssid));
  html.replace("%PASS%", String(password));
  html.replace("%MQTT%", String(mqtt_server));
  return html;
}

void startAPMode() {
  isAPMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config", "12345678");
  Serial.println("AP Mode started");
  Serial.print("AP IP: "); 
  Serial.println(WiFi.softAPIP());

  if (oledInitialized) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("AP Config Mode");
    display.println("SSID: ESP32_Config");
    display.println("Pass: 12345678");
    display.println("IP: 192.168.4.1");
    display.display();
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", getConfigPage());
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasArg("ssid") && request->hasArg("pass") && request->hasArg("mqtt")) {
      String newSsid = request->arg("ssid");
      String newPass = request->arg("pass");
      String newMqtt = request->arg("mqtt");
      saveCredentials(newSsid.c_str(), newPass.c_str(), newMqtt.c_str());
      request->send(200, "text/html", "<html><body><h1>Saved! Restarting...</h1></body></html>");
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    } else {
      request->send(400, "text/html", "<html><body><h1>Invalid data</h1></body></html>");
    }
  });

  server.begin();
}

bool reconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, attempting to reconnect...");
    if (!setup_wifi()) {
      failedReconnectAttempts++;
      return false;
    }
  }

  client.disconnect();
  int retryCount = 0;
  while (!client.connected() && retryCount < 5) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
      failedReconnectAttempts = 0;
      return true;
    }
    Serial.printf("failed, rc=%d try again in 2 seconds\n", client.state());
    vTaskDelay(pdMS_TO_TICKS(500)); // Đọc nhanh hơn
    retryCount++;
  }

  failedReconnectAttempts++;
  return false;
}

void publishMQTTSensor() {
  if (!client.connected() && !reconnect()) {
    Serial.println("MQTT disconnected, cannot publish sensor data!");
    return;
  }

  snprintf(msg, 50, "%.1f", avgTemperature);
  client.publish(TOPIC_STATE_TEMPERATURE, msg);
  
  snprintf(msg, 50, "%.1f", avgHumidity);
  client.publish(TOPIC_STATE_HUMIDITY, msg);
  
  snprintf(msg, 50, "%.1f", avgPressure);
  client.publish(TOPIC_STATE_PRESSURE, msg);

  snprintf(msg, 50, "%.1f", avgCO2);
  client.publish(TOPIC_STATE_CO2, msg);
}

void TaskReadSensor(void *pvParameters) {
  // Wait for MQ135 warmup
  Serial.println("Chờ 20 giây cho MQ135 khởi động...");
  vTaskDelay(pdMS_TO_TICKS(20000));
  Serial.println("MQ135 sẵn sàng!");

  while (1) {
    sensors_event_t humidity_event, temp_event;
    
    if (ahtInitialized && aht.getEvent(&humidity_event, &temp_event)) {
      temperature = temp_event.temperature;
      humidity = humidity_event.relative_humidity;
    } else {
      temperature = humidity = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    if (bmpInitialized) {
      pressure = bmp.readPressure() / 100.0;  // Convert Pa to hPa
      if (isnan(pressure)) {
        pressure = 0;
      }
    }

    co2ppm = readCO2();

    if (temperature != 0 && humidity != 0) {
      tempSum += temperature;
      humiSum += humidity;
      if (pressure != 0) pressSum += pressure;
      if (co2ppm > 0) co2Sum += co2ppm;
      count++;
    }
    countTime++;

    Serial.printf("Temperature: %.1f°C, Humidity: %.1f%%, Pressure: %.1f hPa, CO2: %.1f ppm (%s)\n",
                 temperature, humidity, pressure, co2ppm, getCO2Quality(co2ppm).c_str());

    vTaskDelay(pdMS_TO_TICKS(1000)); // Chậm lại để dễ đọc
  }
}

void TaskPrintData(void *pvParameters) {
  while (1) {
    if (countTime >= 3) {  // Trung bình mỗi 3 giây
      if (count > 0) {
        avgTemperature = tempSum / count;
        avgHumidity = humiSum / count;
        avgPressure = pressSum / count;
        avgCO2 = co2Sum / count;
        
        Serial.println("\n=== 30-Second Averages ===");
        Serial.printf("Temperature: %.1f°C\n", avgTemperature);
        Serial.printf("Humidity: %.1f%%\n", avgHumidity);
        Serial.printf("Pressure: %.1f hPa\n", avgPressure);
        Serial.printf("CO2: %.1f ppm (%s)\n", avgCO2, getCO2Quality(avgCO2).c_str());
        Serial.println("=========================\n");

        updateDisplay = true;
        publishSensorFlag = true;

        tempSum = humiSum = pressSum = co2Sum = 0;
        count = countTime = 0;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1500));
  }
}

void TaskDisplay(void *pvParameters) {
  while (1) {
    if (!isAPMode && updateDisplay && oledInitialized) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      
      // Temperature
      display.setCursor(0, 0);
      display.printf("Temp: %.1f C\n", avgTemperature);
      
      // Humidity
      display.setCursor(0, 16);
      display.printf("Humi: %.1f %%\n", avgHumidity);
      
      // Pressure
      display.setCursor(0, 32);
      display.printf("Press: %.0f hPa\n", avgPressure);
      
      // CO2 with quality
      display.setCursor(0, 48);
      display.printf("CO2: %d ppm (%s)", (int)avgCO2, getCO2Quality(avgCO2).c_str());
      
      display.display();
      updateDisplay = false;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void TaskMQTT(void *pvParameters) {
  unsigned long lastReconnectAttempt = 0;
  const unsigned long reconnectInterval = 5000;

  while (1) {
    unsigned long currentMillis = millis();

    if (!isAPMode) {
      if (!client.connected()) {
        if (currentMillis - lastReconnectAttempt >= reconnectInterval) {
          if (reconnect()) {
            lastReconnectAttempt = 0;
          } else {
            lastReconnectAttempt = currentMillis;
            if (failedReconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
              Serial.println("Too many failed attempts, switching to AP mode");
              startAPMode();
            }
          }
        }
      } else {
        client.loop();
        
        if (publishSensorFlag) {
          publishMQTTSensor();
          publishSensorFlag = false;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup() {
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(1000));

  // Initialize MQ135 pin
  pinMode(MQ135_PIN, INPUT);

  Wire.begin();
  scanI2C();
  
  oledInitialized = initializeOLED();
  
  int attempts = 0;
  while (!aht.begin() && attempts < 3) {
    Serial.println("Could not find AHT20! Retrying...");
    attempts++;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  ahtInitialized = aht.begin();
  
  attempts = 0;
  while (!bmp.begin(0x77) && attempts < 3) {
    Serial.println("Could not find BMP280! Retrying...");
    attempts++;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  bmpInitialized = bmp.begin(0x77);

  if (oledInitialized) {
    display.clearDisplay();
    display.setTextSize(1);
    display.println("System starting...");
    display.println("Wait for MQ135...");
    display.display();
  }

  loadCredentials();
  
  if (setup_wifi()) {
    client.setServer(mqtt_server, mqtt_port);
    if (!reconnect()) {
      Serial.println("Initial MQTT connection failed");
    }
  } else {
    Serial.println("Initial WiFi connection failed");
    startAPMode();
  }

  // Create tasks
  xTaskCreatePinnedToCore(TaskReadSensor, "ReadSensor", 4096, NULL, 3, &TaskReadHandle, 1);
  xTaskCreatePinnedToCore(TaskPrintData, "PrintData", 4096, NULL, 1, &TaskPrintHandle, 1);
  xTaskCreatePinnedToCore(TaskDisplay, "Display", 4096, NULL, 1, &TaskDisplayHandle, 0);
  xTaskCreatePinnedToCore(TaskMQTT, "MQTT", 4096, NULL, 1, &TaskMQTTHandle, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
