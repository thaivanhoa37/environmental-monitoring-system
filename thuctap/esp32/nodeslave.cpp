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

// Khai báo các topic dưới dạng hằng số
const char* TOPIC_STATE_DUST = "node1/state/dust";
const char* TOPIC_STATE_AQI = "node1/state/aqi";
const char* TOPIC_STATE_TEMPERATURE = "node1/state/temperature";
const char* TOPIC_STATE_HUMIDITY = "node1/state/humidity";
const char* TOPIC_STATE_MODE = "node1/state/mode";
const char* TOPIC_STATE_DEVICE1 = "node1/state/device1";
const char* TOPIC_STATE_DEVICE2 = "node1/state/device2";
const char* TOPIC_STATE_AQI_THRESHOLD = "node1/state/aqi_threshold";
const char* TOPIC_STATE_TEMP_THRESHOLD = "node1/state/temp_threshold";

const char* TOPIC_CONTROL_MODE = "node1/control/mode";
const char* TOPIC_CONTROL_DEVICE1 = "node1/control/device1";
const char* TOPIC_CONTROL_DEVICE2 = "node1/control/device2";
const char* TOPIC_CONTROL_AQI_THRESHOLD = "node1/control/aqi_threshold";
const char* TOPIC_CONTROL_TEMP_THRESHOLD = "node1/control/temp_threshold";

#define measurePin  34
#define ledPower    25

#define DHTPIN 4
#define DHTTYPE DHT11

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define BUTTON_MODE 13
#define BUTTON_DEVICE1 26
#define BUTTON_DEVICE2 27

#define DEVICE1_PIN 16
#define DEVICE2_PIN 17

#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 32
#define MQTT_IP_ADDR 64

DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
AsyncWebServer server(80);

#define samplingTime  280
#define deltaTime     40
#define sleepTime     9680

float voMeasured = 0;
float calcVoltage = 0;
float dustDensity = 0;
int AQI = 0;
float temperature = 0, humidity = 0;

float dustDensitySum = 0, tempSum = 0, humSum = 0;
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

// Thêm mutex cho các biến toàn cục
portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;

SemaphoreHandle_t mqttMutex;

float avgDustDensity = 0;
int avgAQI = 0;
float avgTemperature = 0;
float avgHumidity = 0;
bool updateDisplay = false;
bool isAutoMode = false;
bool device1State = false;
bool device2State = false;
bool dataUpdated = false;
bool publishSensorFlag = false;
bool publishDeviceFlag = false;
bool thresholdUpdated = false;

bool isAPMode = false;

float aqiThreshold = 50.0;
float tempThreshold = 28.0;

char ssid[32] = "your_ssid";
char password[32] = "your_password";
char mqtt_server[32] = "your_mqtt_server";
const int mqtt_port = 1883;
const char* mqtt_client_id = "ESP32_Client1";

char msg[50];

int failedReconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 10;
const unsigned long RESET_TIMEOUT = 120000; // 2 phút

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
  return NAN;
}

bool readButton(int pin) {
  return digitalRead(pin) == LOW;
}

void loadCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(SSID_ADDR, ssid);
  EEPROM.get(PASS_ADDR, password);
  EEPROM.get(MQTT_IP_ADDR, mqtt_server);
  EEPROM.end();
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

bool setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  static unsigned long lastAttempt = 0; // Biến tĩnh để theo dõi lần thử cuối
  unsigned long currentMillis = millis();

  // Cơ chế backoff: thời gian chờ tăng theo số lần thử
  if (currentMillis - lastAttempt < (failedReconnectAttempts * 1000)) {
    return false; // Chưa đến thời gian thử lại
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

String getConfigPage() {
  String html = "<!DOCTYPE html><html><body><h1>ESP32 Config node1</h1>";
  html += "<form method='POST' action='/save'>";
  html += "WiFi SSID: <input type='text' name='ssid' value='" + String(ssid) + "'><br>";
  html += "Password: <input type='text' name='pass' value='" + String(password) + "'><br>";
  html += "MQTT Server: <input type='text' name='mqtt' value='" + String(mqtt_server) + "'><br>";
  html += "<input type='submit' value='Save'>";
  html += "</form></body></html>";
  return html;
}

void startAPMode() {
  isAPMode = true;

  // Tắt các thiết bị trước khi vào AP mode
  taskENTER_CRITICAL(&dataMux);
  device1State = false;
  device2State = false;
  digitalWrite(DEVICE1_PIN, LOW); // Tắt thiết bị 1
  digitalWrite(DEVICE2_PIN, LOW); // Tắt thiết bị 2
  isAutoMode = false;
  taskEXIT_CRITICAL(&dataMux);

  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config1", "12345678");
  Serial.println("AP Mode started");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Hiển thị thông tin trên OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("AP Mode Active");
  display.println("Connect to:");
  display.println("SSID: ESP32_Config1");
  display.println("Pass: 12345678");
  display.println("IP: 192.168.4.1");
  display.println("Open: 192.168.4.1");
  display.display();
  Serial.println("OLED display initialized for AP Mode");

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
      isAPMode = false;
      ESP.restart();
    } else {
      request->send(400, "text/html", "<html><body><h1>Invalid input</h1></body></html>");
    }
  });

  server.begin();
  Serial.println("Async Web Server started");

  // Tạm dừng các tác vụ khác, nhưng KHÔNG tạm dừng TaskResetTimeout
  vTaskSuspend(TaskReadHandle);
  vTaskSuspend(TaskPrintHandle);
  vTaskSuspend(TaskDisplayHandle);
  vTaskSuspend(TaskControlHandle);
  vTaskSuspend(TaskMQTTHandle);
  vTaskSuspend(TaskMQTTSubscribeHandle);

  // Thêm timeout cho AP mode
  unsigned long apStartTime = millis();
  while (isAPMode && millis() - apStartTime < 300000) { // Timeout sau 5 phút
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  isAPMode = false;

  // Khôi phục các tác vụ
  vTaskResume(TaskReadHandle);
  vTaskResume(TaskPrintHandle);
  vTaskResume(TaskDisplayHandle);
  vTaskResume(TaskControlHandle);
  vTaskResume(TaskMQTTHandle);
  vTaskResume(TaskMQTTSubscribeHandle);

  display.clearDisplay();
  display.display();
}

void resetWiFiClient() {
  espClient.stop(); // Chỉ dừng kết nối, không tạo lại
}

bool reconnect() {
  if (isAPMode) {
    Serial.println("reconnect: Skipped, system is in AP mode");
    return false;
  }

  if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("reconnect: WiFi not connected, attempting to reconnect...");
      if (!setup_wifi()) {
        failedReconnectAttempts++;
        Serial.print("reconnect: Failed reconnect attempts: ");
        Serial.println(failedReconnectAttempts);
        if (failedReconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
          Serial.println("reconnect: Too many failed attempts, switching to AP mode");
          xSemaphoreGive(mqttMutex);
          startAPMode();
        }
        xSemaphoreGive(mqttMutex);
        return false;
      }
      Serial.println("reconnect: WiFi reconnected");
      resetWiFiClient();
    }

    Serial.println("reconnect: Disconnecting MQTT client");
    client.disconnect();
    resetWiFiClient();

    int retryCount = 0;
    while (!client.connected() && retryCount < 3) {
      Serial.print("reconnect: Attempting MQTT connection (attempt ");
      Serial.print(retryCount + 1);
      Serial.println(")...");
      if (client.connect(mqtt_client_id)) {
        Serial.println("reconnect: MQTT connected");
        client.subscribe(TOPIC_CONTROL_MODE);
        client.subscribe(TOPIC_CONTROL_DEVICE1);
        client.subscribe(TOPIC_CONTROL_DEVICE2);
        client.subscribe(TOPIC_CONTROL_AQI_THRESHOLD);
        client.subscribe(TOPIC_CONTROL_TEMP_THRESHOLD);
        failedReconnectAttempts = 0;
        xSemaphoreGive(mqttMutex);
        return true;
      } else {
        Serial.print("reconnect: MQTT connection failed, rc=");
        int rc = client.state();
        Serial.print(rc);
        if (rc == -2) {
          Serial.println(" (Broker unreachable)");
        } else {
          Serial.println();
        }
        Serial.println(" try again in 2 seconds");
        vTaskDelay(pdMS_TO_TICKS(2000));
        retryCount++;
      }
    }

    failedReconnectAttempts++;
    Serial.print("reconnect: Failed reconnect attempts: ");
    Serial.println(failedReconnectAttempts);
    if (failedReconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
      Serial.println("reconnect: Too many failed attempts, switching to AP mode");
      xSemaphoreGive(mqttMutex);
      startAPMode();
    }
    xSemaphoreGive(mqttMutex);
    return false;
  }
  Serial.println("reconnect: Failed to acquire mutex");
  return false;
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

    Serial.println("MQTT sensor data published");
    xSemaphoreGive(mqttMutex);
  }
}

void publishMQTTDevice() {
  if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    if (!client.connected()) {
      if (!reconnect()) {
        xSemaphoreGive(mqttMutex);
        return;
      }
    }
    client.loop();

    client.publish(TOPIC_STATE_MODE, isAutoMode ? "ON" : "OFF");
    client.publish(TOPIC_STATE_DEVICE1, device1State ? "ON" : "OFF");
    client.publish(TOPIC_STATE_DEVICE2, device2State ? "ON" : "OFF");
    snprintf(msg, 50, "%.1f", aqiThreshold);
    client.publish(TOPIC_STATE_AQI_THRESHOLD, msg);
    snprintf(msg, 50, "%.1f", tempThreshold);
    client.publish(TOPIC_STATE_TEMP_THRESHOLD, msg);

    Serial.println("MQTT device state published");
    xSemaphoreGive(mqttMutex);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String messageTemp;

  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }

  if (topicStr == TOPIC_CONTROL_MODE) {
    isAutoMode = (messageTemp == "ON");
    Serial.print("Mode updated via MQTT to: "); Serial.println(isAutoMode ? "Auto" : "Manual");
    if (!isAutoMode) {
      device1State = false;
      device2State = false;
      digitalWrite(DEVICE1_PIN, device1State);
      digitalWrite(DEVICE2_PIN, device2State);
      publishDeviceFlag = true;
    }
    updateDisplay = true;
    if (isAutoMode) {
      dataUpdated = true;
    }
  }
  else if (topicStr == TOPIC_CONTROL_DEVICE1 && !isAutoMode) {
    device1State = (messageTemp == "ON");
    digitalWrite(DEVICE1_PIN, device1State);
    Serial.print("Device 1 updated via MQTT to: "); Serial.println(device1State ? "ON" : "OFF");
    updateDisplay = true;
    publishDeviceFlag = true;
  }
  else if (topicStr == TOPIC_CONTROL_DEVICE2 && !isAutoMode) {
    device2State = (messageTemp == "ON");
    digitalWrite(DEVICE2_PIN, device2State);
    Serial.print("Device 2 updated via MQTT to: "); Serial.println(device2State ? "ON" : "OFF");
    updateDisplay = true;
    publishDeviceFlag = true;
  }
  else if (topicStr == TOPIC_CONTROL_AQI_THRESHOLD) {
    aqiThreshold = messageTemp.toFloat();
    Serial.print("AQI Threshold updated via MQTT to: "); Serial.println(aqiThreshold);
    thresholdUpdated = true;
    dataUpdated = true;
  }
  else if (topicStr == TOPIC_CONTROL_TEMP_THRESHOLD) {
    tempThreshold = messageTemp.toFloat();
    Serial.print("Temperature Threshold updated via MQTT to: "); Serial.println(tempThreshold);
    thresholdUpdated = true;
    dataUpdated = true;
  }
}

void TaskReadSensor(void *pvParameters) {
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

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("❌ Lỗi đọc cảm biến DHT11!");
    }

    if (dustDensity > 0 && !isnan(temperature) && !isnan(humidity)) {
      dustDensitySum += dustDensity;
      tempSum += temperature;
      humSum += humidity;
      count++;
    }
    countTime++;

    Serial.print("Giá trị đo được: ");
    Serial.print("Nồng độ bụi: ");
    Serial.print(dustDensity);
    Serial.print(" µg/m³ | AQI: ");
    Serial.println(AQI);

    Serial.print("🌡 Nhiệt độ: ");
    Serial.print(temperature);
    Serial.print("°C | 💧 Độ ẩm: ");
    Serial.print(humidity);
    Serial.println("%");
    Serial.println("-------------------------");

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
        taskEXIT_CRITICAL(&dataMux);

        Serial.println("--------------");
        Serial.println("Trung bình 30s:");
        Serial.print("Trung bình nồng độ bụi: ");
        Serial.print(avgDustDensity);
        Serial.print(" µg/m³ | AQI: ");
        Serial.println(avgAQI);

        Serial.print("Trung bình nhiệt độ: ");
        Serial.print(avgTemperature);
        Serial.print("°C | Độ ẩm trung bình: ");
        Serial.print(avgHumidity);
        Serial.println("%");
        Serial.println("--------------");

        taskENTER_CRITICAL(&dataMux);
        updateDisplay = true;
        dataUpdated = true;
        publishSensorFlag = true;
        publishDeviceFlag = true;
        taskEXIT_CRITICAL(&dataMux);

        dustDensitySum = 0;
        tempSum = 0;
        humSum = 0;
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

    // Làm mới định kỳ mỗi 60 giây để tránh burn-in
    if (millis() - lastRefresh > 60000) {
      localUpdateDisplay = true;
      lastRefresh = millis();
    }

    if (!isAPMode && localUpdateDisplay) {
      float localDustDensity, localTemperature, localHumidity;
      int localAQI;
      bool localDevice1State, localDevice2State, localIsAutoMode;

      // Bảo vệ truy cập biến toàn cục
      taskENTER_CRITICAL(&dataMux);
      localDustDensity = avgDustDensity;
      localAQI = avgAQI;
      localTemperature = avgTemperature;
      localHumidity = avgHumidity;
      localDevice1State = device1State;
      localDevice2State = device2State;
      localIsAutoMode = isAutoMode;
      updateDisplay = false;
      taskEXIT_CRITICAL(&dataMux);

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);

      display.print("Mode: "); display.println(localIsAutoMode ? "Auto" : "Manual");
      display.print("PM2.5: "); display.print(localDustDensity, 1); display.println(" ug/m3");
      display.print("AQI: "); display.println(localAQI);
      display.print("Temp: "); display.print(localTemperature, 1); display.println(" C");
      display.print("Hum: "); display.print(localHumidity, 1); display.println(" %");
      display.print("Dev1: "); display.println(localDevice1State ? "ON" : "OFF");
      display.print("Dev2: "); display.println(localDevice2State ? "ON" : "OFF");

      display.display();
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void TaskControl(void *pvParameters) {
  pinMode(BUTTON_MODE, INPUT_PULLUP);
  pinMode(BUTTON_DEVICE1, INPUT_PULLUP);
  pinMode(BUTTON_DEVICE2, INPUT_PULLUP);
  pinMode(DEVICE1_PIN, OUTPUT);
  pinMode(DEVICE2_PIN, OUTPUT);

  // Đảm bảo trạng thái ban đầu của thiết bị
  digitalWrite(DEVICE1_PIN, device1State);
  digitalWrite(DEVICE2_PIN, device2State);

  unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 200;

  while (1) {
    unsigned long currentTime = millis();

    if (readButton(BUTTON_MODE) && (currentTime - lastDebounceTime) > debounceDelay) {
      taskENTER_CRITICAL(&dataMux);
      isAutoMode = !isAutoMode;
      publishDeviceFlag = true;
      updateDisplay = true;
      taskEXIT_CRITICAL(&dataMux);

      Serial.print("Mode switched to: "); Serial.println(isAutoMode ? "Auto" : "Manual");
      
      // Xử lý nút MODE
      if (isAutoMode) {
        bool prevDevice1, prevDevice2;

        taskENTER_CRITICAL(&dataMux);
        prevDevice1 = device1State;
        prevDevice2 = device2State;
        device1State = (avgTemperature > tempThreshold);
        device2State = (avgAQI > aqiThreshold);
        taskEXIT_CRITICAL(&dataMux);

        digitalWrite(DEVICE1_PIN, device1State);
        digitalWrite(DEVICE2_PIN, device2State);

        if (prevDevice1 != device1State || prevDevice2 != device2State) {
          Serial.print("Auto Mode - Temp: "); Serial.print(avgTemperature);
          Serial.print("C, AQI: "); Serial.println(avgAQI);
          Serial.print("Device 1: "); Serial.print(device1State ? "ON" : "OFF");
          Serial.print(", Device 2: "); Serial.println(device2State ? "ON" : "OFF");
        }
      } else {
        taskENTER_CRITICAL(&dataMux);
        device1State = false;
        device2State = false;
        taskEXIT_CRITICAL(&dataMux);

        digitalWrite(DEVICE1_PIN, device1State);
        digitalWrite(DEVICE2_PIN, device2State);
      }

      lastDebounceTime = currentTime;
    }

    bool localIsAutoMode;
    taskENTER_CRITICAL(&dataMux);
    localIsAutoMode = isAutoMode;
    taskEXIT_CRITICAL(&dataMux);

    // Xử lý nút điều khiển thiết bị ở chế độ manual
    if (!localIsAutoMode) {
      if (readButton(BUTTON_DEVICE1) && (currentTime - lastDebounceTime) > debounceDelay) {
        taskENTER_CRITICAL(&dataMux);
        device1State = !device1State;
        updateDisplay = true;
        publishDeviceFlag = true;
        taskEXIT_CRITICAL(&dataMux);

        digitalWrite(DEVICE1_PIN, device1State);
        Serial.print("Device 1 switched to: "); Serial.println(device1State ? "ON" : "OFF");
        lastDebounceTime = currentTime;
      }

      if (readButton(BUTTON_DEVICE2) && (currentTime - lastDebounceTime) > debounceDelay) {
        taskENTER_CRITICAL(&dataMux);
        device2State = !device2State;
        updateDisplay = true;
        publishDeviceFlag = true;
        taskEXIT_CRITICAL(&dataMux);

        digitalWrite(DEVICE2_PIN, device2State);
        Serial.print("Device 2 switched to: "); Serial.println(device2State ? "ON" : "OFF");
        lastDebounceTime = currentTime;
      }
    } else {
      bool prevDevice1, prevDevice2;

      taskENTER_CRITICAL(&dataMux);
      prevDevice1 = device1State;
      prevDevice2 = device2State;

      if(avgTemperature > tempThreshold){
        device1State = true;
      } else if(avgTemperature < tempThreshold - 3){
        device1State = false;
      }

      if (avgAQI > aqiThreshold) {
        device2State = true; 
      } else if (avgAQI < aqiThreshold - 10) {
        device2State = false;
      }
      taskEXIT_CRITICAL(&dataMux);

      if (prevDevice1 != device1State || prevDevice2 != device2State) {
        digitalWrite(DEVICE1_PIN, device1State);
        digitalWrite(DEVICE2_PIN, device2State);

        Serial.print("Auto Mode - Temp: "); Serial.print(avgTemperature);
        Serial.print("C, AQI: "); Serial.println(avgAQI);
        Serial.print("Device 1: "); Serial.print(device1State ? "ON" : "OFF");
        Serial.print(", Device 2: "); Serial.println(device2State ? "ON" : "OFF");

        taskENTER_CRITICAL(&dataMux);
        publishDeviceFlag = true;
        updateDisplay = true;
        taskEXIT_CRITICAL(&dataMux);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void TaskMQTT(void *pvParameters) {
  unsigned long lastReconnectAttempt = 0;
  const unsigned long reconnectInterval = 2000;

  while (1) {
    if (isAPMode) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    unsigned long currentMillis = millis();

    if (WiFi.status() != WL_CONNECTED || !client.connected()) {
      Serial.println("Connection lost, attempting to reconnect...");
      if (currentMillis - lastReconnectAttempt >= reconnectInterval) {
        if (!reconnect()) {
          Serial.println("Reconnection failed");
          if (failedReconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
            Serial.println("Too many failed attempts, switching to AP mode");
            startAPMode();
          }
        }
        lastReconnectAttempt = currentMillis;
      }
    } else {
      lastReconnectAttempt = currentMillis;
    }

    if (publishSensorFlag) {
      publishMQTTSensor();
      publishSensorFlag = false;
    }

    if (publishDeviceFlag) {
      publishMQTTDevice();
      publishDeviceFlag = false;
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void TaskResetTimeout(void *pvParameters) {
  unsigned long connectionFailureStart = 0;
  bool connectionLost = false;

  while (1) {
    if (isAPMode) {
      // Nếu ở AP mode, kiểm tra thời gian kể từ khi vào AP mode
      if (!connectionLost) {
        connectionLost = true;
        connectionFailureStart = millis();
        Serial.println("AP mode detected, starting timeout countdown...");
      }
      if (millis() - connectionFailureStart >= RESET_TIMEOUT) {
        Serial.println("AP mode timeout, resetting ESP32...");
        // Tắt các thiết bị trước khi reset
        taskENTER_CRITICAL(&dataMux);
        device1State = false;
        device2State = false;
        digitalWrite(DEVICE1_PIN, LOW);
        digitalWrite(DEVICE2_PIN, LOW);
        isAutoMode = false;
        taskEXIT_CRITICAL(&dataMux);
        ESP.restart();
      }
    } else if (WiFi.status() != WL_CONNECTED || !client.connected()) {
      // Nếu không ở AP mode nhưng mất kết nối WiFi hoặc MQTT
      if (!connectionLost) {
        connectionLost = true;
        connectionFailureStart = millis();
        Serial.println("Connection lost detected, starting timeout countdown...");
      }
      if (millis() - connectionFailureStart >= RESET_TIMEOUT) {
        Serial.println("Connection timeout, resetting ESP32...");
        // Tắt các thiết bị trước khi reset
        taskENTER_CRITICAL(&dataMux);
        device1State = false;
        device2State = false;
        digitalWrite(DEVICE1_PIN, LOW);
        digitalWrite(DEVICE2_PIN, LOW);
        isAutoMode = false;
        taskEXIT_CRITICAL(&dataMux);
        ESP.restart();
      }
    } else {
      // Nếu kết nối được khôi phục
      if (connectionLost) {
        Serial.println("Connection restored, resetting timeout...");
        connectionLost = false;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void TaskMQTTSubscribe(void *pvParameters) {
  client.setCallback(callback);

  while (1) {
    if (isAPMode) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      if (!client.connected()) {
        if (!reconnect()) {
          xSemaphoreGive(mqttMutex);
          vTaskDelay(pdMS_TO_TICKS(5000));
          continue;
        }
      }
      client.loop();
      xSemaphoreGive(mqttMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPower, OUTPUT);
  dht.begin();
  mqttMutex = xSemaphoreCreateMutex();

  // Khởi tạo trạng thái ban đầu của các thiết bị
  device1State = false;
  device2State = false;
  digitalWrite(DEVICE1_PIN, LOW); // Tắt thiết bị 1
  digitalWrite(DEVICE2_PIN, LOW); // Tắt thiết bị 2
  isAutoMode = false; // Bắt đầu ở chế độ manual

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Khoi dong...");
  display.display();
  vTaskDelay(pdMS_TO_TICKS(2000));

  loadCredentials();

  xTaskCreatePinnedToCore(TaskResetTimeout, "TaskReset", 2048, NULL, 2, NULL, 1);

  bool connectionSuccess = false;
  while (!connectionSuccess) {
    if (setup_wifi()) {
      client.setServer(mqtt_server, mqtt_port);
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

void loop() {}