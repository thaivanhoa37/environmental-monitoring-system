#include <Arduino.h>
#include <DHT.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Adafruit_BMP280.h>  // For BMP280
#include <Adafruit_AHTX0.h>   // For AHT20
#include <MQUnifiedsensor.h>  // For MQ135

// Function declarations
String getConfigPage();
bool setup_wifi();
bool reconnect();
void startAPMode();
void TaskControl(void *pvParameters);
void TaskMQTT(void *pvParameters);
void TaskMQTTSubscribe(void *pvParameters);
void TaskResetTimeout(void *pvParameters);
void publishMQTTSensor();
void mqtt_callback(char* topic, byte* payload, unsigned int length);

// Topic definitions
const char* TOPIC_STATE_DUST = "node1/state/dust";
const char* TOPIC_STATE_AQI = "node1/state/aqi";
const char* TOPIC_STATE_TEMPERATURE = "node1/state/temperature";
const char* TOPIC_STATE_HUMIDITY = "node1/state/humidity";
const char* TOPIC_STATE_CO2 = "node1/state/co2";          
const char* TOPIC_STATE_PRESSURE = "node1/state/pressure"; 
const char* TOPIC_STATE_ALT = "node1/state/altitude";     

#define measurePin  13
#define ledPower    12

#define DHTPIN 4
#define DHTTYPE DHT11

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 32
#define MQTT_IP_ADDR 64

// MQ135 settings
#define Board "ESP-32"
#define Type "MQ-135"
#define Voltage_Resolution 3.3
#define ADC_Bit_Resolution 12
#define RatioMQ135CleanAir 3.6
#define R0 10
#define MQ135_PIN 34

// MQ135 Calibration data
#define CALIBRATION_SAMPLES 50
#define CALIBRATION_SAMPLE_INTERVAL 500
#define MQ135_A 102.2
#define MQ135_B -2.473

DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
AsyncWebServer server(80);
Adafruit_BMP280 bmp;
Adafruit_AHTX0 aht;
MQUnifiedsensor mq135(Board, Voltage_Resolution, ADC_Bit_Resolution, MQ135_PIN, Type);

#define samplingTime  280
#define deltaTime     40
#define sleepTime     9680

float voMeasured = 0;
float calcVoltage = 0;
float dustDensity = 0;
int AQI = 0;
float temperature = 0, humidity = 0;

// Sensor variables
float co2ppm = 0;
float pressure = 0;
float altitude = 0;
float aht20Temperature = 0;
float aht20Humidity = 0;
float bmp280Temperature = 0;

float dustDensitySum = 0, tempSum = 0, humSum = 0;
float co2Sum = 0, pressureSum = 0, altitudeSum = 0;
int count = 0;
int countTime = 0;

TaskHandle_t TaskReadHandle;
TaskHandle_t TaskPrintHandle;
TaskHandle_t TaskDisplayHandle;
TaskHandle_t TaskControlHandle;
TaskHandle_t TaskMQTTHandle;
TaskHandle_t TaskMQTTSubscribeHandle;
TaskHandle_t TaskResetHandle;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t mqttMutex;

float avgDustDensity = 0;
int avgAQI = 0;
float avgTemperature = 0;
float avgHumidity = 0;
float avgCO2 = 0;
float avgPressure = 0;
float avgAltitude = 0;

bool updateDisplay = false;
bool dataUpdated = false;
bool publishSensorFlag = false;
bool isAPMode = false;

char ssid[32] = "vanhoa";
char password[32] = "11111111";
char mqtt_server[32] = "your_mqtt_server";
const int mqtt_port = 1883;
const char* mqtt_client_id = "ESP32_Client1";

char msg[50];

int failedReconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 10;
const unsigned long RESET_TIMEOUT = 120000;

WiFiClient espClient;
PubSubClient client(espClient);

struct AQILevel {
  float Clow, Chigh;
  int Ilow, Ihigh;
};

AQILevel aqiTable[] = {
  {0.0, 12.0, 0, 50},
  {12.1, 35.4, 51, 100},
  {35.5, 55.4, 101, 150},
  {55.5, 150.4, 151, 200},
  {150.5, 250.4, 201, 300},
  {250.5, 500.4, 301, 500}
};

int calculateAQI(float pm25) {
  if (pm25 <= 0) return 0;
  if (pm25 > 500) return 500;

  for (int i = 0; i < 6; i++) {
    if (pm25 >= aqiTable[i].Clow && pm25 <= aqiTable[i].Chigh) {
      return ((aqiTable[i].Ihigh - aqiTable[i].Ilow) / (aqiTable[i].Chigh - aqiTable[i].Clow)) *
             (pm25 - aqiTable[i].Clow) + aqiTable[i].Ilow;
    }
  }
  return -1;
}

void loadCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(SSID_ADDR, ssid);
  EEPROM.get(PASS_ADDR, password);
  EEPROM.get(MQTT_IP_ADDR, mqtt_server);
  EEPROM.end();
}

String getConfigPage() {
  String html = "<!DOCTYPE html><html><body><h1>ESP32 Config node1</h1>";
  html += "<form method='POST' action='/save'>";
  html += "WiFi SSID: <input type='text' name='ssid' value='" + String(ssid) + "'><br><br>";
  html += "Password: <input type='password' name='pass' value='" + String(password) + "'><br><br>";
  html += "MQTT Server: <input type='text' name='mqtt' value='" + String(mqtt_server) + "'><br><br>";
  html += "<input type='submit' value='Save and Restart'>";
  html += "</form></body></html>";
  return html;
}

void saveCredentials(const char* newSsid, const char* newPass, const char* newMqtt) {
  char tempSsid[32];
  char tempPass[32];
  char tempMqtt[32];

  strncpy(tempSsid, newSsid, 32);
  strncpy(tempPass, newPass, 32);
  strncpy(tempMqtt, newMqtt, 32);

  tempSsid[31] = '\0';
  tempPass[31] = '\0';
  tempMqtt[31] = '\0';

  if (strcmp(newSsid, ssid) != 0 || strcmp(newPass, password) != 0 || strcmp(newMqtt, mqtt_server) != 0) {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(SSID_ADDR, tempSsid);
    EEPROM.put(PASS_ADDR, tempPass);
    EEPROM.put(MQTT_IP_ADDR, tempMqtt);
    EEPROM.commit();
    EEPROM.end();
  }

  strncpy(ssid, tempSsid, 32);
  strncpy(password, tempPass, 32);
  strncpy(mqtt_server, tempMqtt, 32);
}

void TaskReadSensor(void *pvParameters) {
  if (!bmp.begin(BMP280_ADDRESS)) {
    Serial.println("Could not find a valid BMP280 sensor!");
  } else {
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
  }

  if (!aht.begin()) {
    Serial.println("Could not find AHT20 sensor!");
  }

  mq135.setRegressionMethod(1);
  mq135.init();
  Serial.print("Calibrating MQ135. Please wait.");
  float calcR0 = 0;
  for(int i = 1; i <= CALIBRATION_SAMPLES; i++) {
    mq135.update();
    calcR0 += mq135.calibrate(RatioMQ135CleanAir);
    if (i % 10 == 0) Serial.print(".");
    delay(CALIBRATION_SAMPLE_INTERVAL);
  }
  mq135.setR0(calcR0/CALIBRATION_SAMPLES);
  mq135.setA(MQ135_A);
  mq135.setB(MQ135_B);
  Serial.println(" Done!");

  while (1) {
    taskENTER_CRITICAL(&mux);
    digitalWrite(ledPower, LOW);
    delayMicroseconds(samplingTime);
    
    voMeasured = analogRead(measurePin);
    delayMicroseconds(deltaTime);
    
    digitalWrite(ledPower, HIGH);
    taskEXIT_CRITICAL(&mux);
    
    delayMicroseconds(sleepTime);

    calcVoltage = voMeasured * (3.3 / 4095.0);
    dustDensity = (170 * calcVoltage - 0.1);

    if (dustDensity < 0) dustDensity = 0;
    AQI = calculateAQI(dustDensity);

    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    sensors_event_t humidity_event, temp_event;
    if (aht.getEvent(&humidity_event, &temp_event)) {
      aht20Temperature = temp_event.temperature;
      aht20Humidity = humidity_event.relative_humidity;
    }

    bmp280Temperature = bmp.readTemperature();
    pressure = bmp.readPressure() / 100.0F;
    altitude = bmp.readAltitude(1013.25);

    mq135.update();
    co2ppm = mq135.readSensor();
    
    temperature = (temperature + aht20Temperature + bmp280Temperature) / 3.0;
    humidity = (humidity + aht20Humidity) / 2.0;

    if (dustDensity > 0 && !isnan(temperature) && !isnan(humidity) && !isnan(co2ppm) && !isnan(pressure)) {
      dustDensitySum += dustDensity;
      tempSum += temperature;
      humSum += humidity;
      co2Sum += co2ppm;
      pressureSum += pressure;
      altitudeSum += altitude;
      count++;
    }
    countTime++;

    Serial.println("=== Sensor Readings ===");
    Serial.printf("Dust: %.2f µg/m³ | AQI: %d\n", dustDensity, AQI);
    Serial.printf("Temp: %.2f°C\n", temperature);
    Serial.printf("Hum: %.2f%%\n", humidity);
    Serial.printf("CO2: %.2f ppm\n", co2ppm);
    Serial.printf("Pre: %.2f hPa\n", pressure);
    Serial.printf("Alt: %.2f m\n", altitude);
    Serial.println("====================");

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void TaskPrintData(void *pvParameters) {
  while (1) {
    if (countTime >= 150) {
      if (count > 0) {
        taskENTER_CRITICAL(&dataMux);
        avgDustDensity = dustDensitySum / count;
        avgTemperature = tempSum / count;
        avgHumidity = humSum / count;
        avgAQI = calculateAQI(avgDustDensity);
        avgCO2 = co2Sum / count;
        avgPressure = pressureSum / count;
        avgAltitude = altitudeSum / count;
        taskEXIT_CRITICAL(&dataMux);

        Serial.println("====== 30s Average ======");
        Serial.printf("Dust: %.2f µg/m³ | AQI: %d\n", avgDustDensity, avgAQI);
        Serial.printf("Temp: %.2f°C\n", avgTemperature);
        Serial.printf("Hum: %.2f%%\n", avgHumidity);
        Serial.printf("CO2: %.2f ppm\n", avgCO2);
        Serial.printf("Pre: %.2f hPa\n", avgPressure);
        Serial.printf("Alt: %.2f m\n", avgAltitude);
        Serial.println("=======================");

        taskENTER_CRITICAL(&dataMux);
        updateDisplay = true;
        dataUpdated = true;
        publishSensorFlag = true;
        taskEXIT_CRITICAL(&dataMux);

        dustDensitySum = 0;
        tempSum = 0;
        humSum = 0;
        co2Sum = 0;
        pressureSum = 0;
        altitudeSum = 0;
        count = 0;
        countTime = 0;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1500));
  }
}

void TaskDisplay(void *pvParameters) {
  while (1) {
    static unsigned long lastRefresh = 0;
    bool localUpdateDisplay;

    taskENTER_CRITICAL(&dataMux);
    localUpdateDisplay = updateDisplay;
    taskEXIT_CRITICAL(&dataMux);

    if (millis() - lastRefresh > 60000) {
      localUpdateDisplay = true;
      lastRefresh = millis();
    }

    if (!isAPMode && localUpdateDisplay) {
      float localDustDensity, localTemperature, localHumidity, localCO2, localPressure;
      int localAQI;

      taskENTER_CRITICAL(&dataMux);
      localDustDensity = avgDustDensity;
      localAQI = avgAQI;
      localTemperature = avgTemperature;
      localHumidity = avgHumidity;
      localCO2 = avgCO2;
      localPressure = avgPressure;
      updateDisplay = false;
      taskEXIT_CRITICAL(&dataMux);

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);

      display.print("PM2.5: "); display.print(localDustDensity, 1); display.println(" ug/m3");
      display.print("AQI: "); display.println(localAQI);
      display.print("Temp: "); display.print(localTemperature, 1); display.println(" C");
      display.print("Hum: "); display.print(localHumidity, 1); display.println(" %");
      display.print("CO2: "); display.print(localCO2, 0); display.println(" ppm");
      display.print("Pre: "); display.print(localPressure, 0); display.println(" hPa");

      display.display();
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

bool setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  static unsigned long lastAttempt = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - lastAttempt < (failedReconnectAttempts * 1000)) {
    return false;
  }

  lastAttempt = currentMillis;

  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.println(WiFi.localIP());
    failedReconnectAttempts = 0;
    return true;
  }
  Serial.println("\nWiFi connection failed");
  failedReconnectAttempts++;
  return false;
}

void startAPMode() {
  isAPMode = true;

  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config1", "12345678");
  Serial.println("AP Mode started");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("AP Mode Active");
  display.println("Connect to:");
  display.println("SSID: ESP32_Config1");
  display.println("Pass: 12345678");
  display.println("IP: 192.168.4.1");
  display.display();

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
      request->send(400, "text/html", "<html><body><h1>Invalid input</h1></body></html>");
    }
  });

  server.begin();

  vTaskSuspend(TaskReadHandle);
  vTaskSuspend(TaskPrintHandle);
  vTaskSuspend(TaskDisplayHandle);
  vTaskSuspend(TaskControlHandle);
  vTaskSuspend(TaskMQTTHandle);
  vTaskSuspend(TaskMQTTSubscribeHandle);
}

void safeRestart() {
  // Clean up and prepare for restart
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();
  
  // Suspend all tasks
  vTaskSuspend(TaskReadHandle);
  vTaskSuspend(TaskPrintHandle);
  vTaskSuspend(TaskDisplayHandle);
  vTaskSuspend(TaskControlHandle);
  vTaskSuspend(TaskMQTTHandle);
  vTaskSuspend(TaskMQTTSubscribeHandle);
  
  // Wait for operations to complete
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  // Clear display
  display.clearDisplay();
  display.display();
  
  Serial.println("Performing safe restart...");
  Serial.flush();
  
  // Final delay before restart
  vTaskDelay(pdMS_TO_TICKS(100));
  
  esp_restart();
}

void TaskResetTimeout(void *pvParameters) {
  unsigned long lastRebootTime = millis();
  const unsigned long REBOOT_INTERVAL = 24 * 60 * 60 * 1000; // 24 hours
  const unsigned long CHECK_INTERVAL = 60000; // Check every minute

  while (1) {
    unsigned long currentTime = millis();
    
    // Force reboot every 24 hours
    if (currentTime - lastRebootTime >= REBOOT_INTERVAL) {
      Serial.println("Scheduled reboot initiated...");
      safeRestart();
    }

    // Check connections every minute
    if (isAPMode || WiFi.status() != WL_CONNECTED || !client.connected()) {
      Serial.println("Connection issue detected, resetting ESP32...");
      safeRestart();
    }

    vTaskDelay(pdMS_TO_TICKS(CHECK_INTERVAL));
  }
}

void TaskControl(void *pvParameters) {
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void publishMQTTSensor() {
  if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    if (!client.connected()) {
      if (!reconnect()) {
        xSemaphoreGive(mqttMutex);
        return;
      }
    }
    client.loop();

    snprintf(msg, 50, "%.1f", avgDustDensity);
    client.publish(TOPIC_STATE_DUST, msg);
    snprintf(msg, 50, "%d", avgAQI);
    client.publish(TOPIC_STATE_AQI, msg);
    snprintf(msg, 50, "%.1f", avgTemperature);
    client.publish(TOPIC_STATE_TEMPERATURE, msg);
    snprintf(msg, 50, "%.1f", avgHumidity);
    client.publish(TOPIC_STATE_HUMIDITY, msg);
    snprintf(msg, 50, "%.1f", avgCO2);
    client.publish(TOPIC_STATE_CO2, msg);
    snprintf(msg, 50, "%.1f", avgPressure);
    client.publish(TOPIC_STATE_PRESSURE, msg);
    snprintf(msg, 50, "%.1f", avgAltitude);
    client.publish(TOPIC_STATE_ALT, msg);

    Serial.println("MQTT sensor data published");
    xSemaphoreGive(mqttMutex);
  }
}

void TaskMQTT(void *pvParameters) {
  while (1) {
    if (!isAPMode && publishSensorFlag) {
      publishMQTTSensor();
      publishSensorFlag = false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  // Empty callback as we don't need to handle incoming messages
}

bool reconnect() {
  int attempts = 0;
  while (!client.connected() && attempts < 3) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
      return true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      attempts++;
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
  return false;
}

void TaskMQTTSubscribe(void *pvParameters) {
  while (1) {
    if (!isAPMode && client.connected()) {
      client.loop();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  pinMode(ledPower, OUTPUT);
  pinMode(MQ135_PIN, INPUT);
  
  dht.begin();
  mqttMutex = xSemaphoreCreateMutex();

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Starting...");
  display.display();
  vTaskDelay(pdMS_TO_TICKS(2000));

  loadCredentials();

  xTaskCreatePinnedToCore(TaskResetTimeout, "TaskReset", 2048, NULL, 2, NULL, 1);

  bool connectionSuccess = false;
  while (!connectionSuccess) {
    if (setup_wifi()) {
      client.setServer(mqtt_server, mqtt_port);
      client.setCallback(mqtt_callback);
      if (reconnect()) {
        connectionSuccess = true;
      } else {
        Serial.println("MQTT connection failed, starting AP mode");
        WiFi.disconnect();
        startAPMode();
      }
    } else {
      Serial.println("WiFi connection failed, starting AP mode");
      startAPMode();
    }
  }

  xTaskCreatePinnedToCore(TaskReadSensor, "TaskRead", 4096, NULL, 3, &TaskReadHandle, 1);
  xTaskCreatePinnedToCore(TaskPrintData, "TaskPrint", 4096, NULL, 1, &TaskPrintHandle, 1);
  xTaskCreatePinnedToCore(TaskDisplay, "TaskDisplay", 4096, NULL, 1, &TaskDisplayHandle, 0);
  xTaskCreatePinnedToCore(TaskControl, "TaskControl", 4096, NULL, 1, &TaskControlHandle, 1);
  xTaskCreatePinnedToCore(TaskMQTT, "TaskMQTT", 8192, NULL, 3, &TaskMQTTHandle, 1);
  xTaskCreatePinnedToCore(TaskMQTTSubscribe, "TaskMQTTSubscribe", 8192, NULL, 2, &TaskMQTTSubscribeHandle, 1);
}

void loop() {
  // Empty as we're using FreeRTOS tasks
}
