# Environmental Monitoring System for Schools and Public Parks

## Introduction

This project builds a real-time environmental monitoring system using an **ESP32** and sensors to measure environmental parameters such as air quality, temperature, humidity, atmospheric pressure, and particulate matter. Data is transmitted via the **MQTT** protocol to a **Raspberry Pi 4**, stored in **InfluxDB**, displayed on a **Grafana** dashboard, and triggers alerts when thresholds are exceeded (e.g., temperature > 35°C, dust > 150 µg/m³).

The project is implemented by student **Thái Văn Hòa** to raise environmental awareness and educate students about **IoT** applications in schools or public parks. The project runs from May 2025 to June 2025.

### Objectives
- Provide real-time environmental data to monitor air pollution, temperature, humidity, pressure, and dust levels.
- Educate students on IoT applications in environmental monitoring.
- Create a cost-effective, easy-to-deploy, and highly practical community product.

## Components

### Hardware
- **ESP32 Dev Module**: Main microcontroller for reading sensor data and sending it via MQTT.
- **Sensors**:
  - **MQ-135**: Measures air quality (CO2, VOCs).
  - **DHT11**: Measures temperature and humidity (low-cost, suitable for educational purposes).
  - **BMP280**: Measures atmospheric pressure (supports weather forecasting).
  - **AHT20**: Measures temperature and humidity (more accurate than DHT11).
  - **GP2Y1014AU0F**: Measures particulate matter (similar to PM2.5).
- **Raspberry Pi 4 (4GB)**: Runs MQTT broker (Mosquitto), Node-RED, InfluxDB, and Grafana.
- **Accessories**: Jumper wires, breadboard, 150µF capacitor, 220Ω resistor (for GP2Y1014AU0F), 5V/3.3V power supply.

### Software
- **Arduino IDE**: For programming the ESP32.
- **Arduino Libraries**:
  - `PubSubClient`: MQTT communication.
  - `DHT`: Supports DHT11.
  - `Adafruit_BMP280`: Supports BMP280.
  - `AHTxx`: Supports AHT20.
- **Raspberry Pi**:
  - **Mosquitto**: MQTT broker.
  - **Node-RED**: Processes data and creates alert flows.
  - **InfluxDB**: Stores real-time data.
  - **Grafana**: Displays dashboards.

### Estimated Cost
- ESP32: 100,000 VND
- MQ-135: 50,000 VND
- DHT11: 30,000 VND
- BMP280: 50,000 VND
- AHT20: 50,000 VND
- GP2Y1014AU0F: 100,000 VND
- Raspberry Pi 4 (4GB): 2,500,000 VND
- Accessories: 50,000 VND
- **Total**: ~1,880,000 VND (~$75 USD)

## System Architecture
1. **ESP32**:
   - Reads data from sensors (MQ-135, DHT11, BMP280, AHT20, GP2Y1014AU0F).
   - Sends data via MQTT to topics: `env/temperature_dht`, `env/humidity_dht`, `env/temperature_aht`, `env/humidity_aht`, `env/air_quality`, `env/pressure`, `env/dust`.
2. **Raspberry Pi 4**:
   - Runs Mosquitto to receive MQTT data.
   - Node-RED processes data, stores it in InfluxDB, and sends alerts (email/Telegram) if thresholds are exceeded.
3. **Grafana**:
   - Displays a dashboard with charts for temperature, humidity, air quality, pressure, and dust levels.

## Setup Instructions

### 1. Hardware Setup
- **Connect sensors to ESP32**:
  - MQ-135: Analog → GPIO 34, VCC → 5V, GND → GND.
  - DHT11: Data → GPIO 4, VCC → 3.3V, GND → GND.
  - BMP280: I2C (SDA → GPIO 21, SCL → GPIO 22), VCC → 3.3V, GND → GND.
  - AHT20: I2C (SDA → GPIO 21, SCL → GPIO 22), VCC → 3.3V, GND → GND.
  - GP2Y1014AU0F: Vout → GPIO 35, LED → GPIO 32, VCC → 5V, GND → GND, add 150µF capacitor and 220Ω resistor.
- **Power Supply**:
  - ESP32: 5V adapter or 3.3V battery.
  - Raspberry Pi: 5V/3A adapter.

### 2. Software Setup
- **On your computer**:
  - Install [Arduino IDE](https://www.arduino.cc/en/software).
  - Add ESP32 support via Board Manager (URL: `https://dl.espressif.com/dl/package_esp32_index.json`).
  - Install Arduino libraries: `PubSubClient`, `DHT`, `Adafruit_BMP280`, `AHTxx`.
- **On Raspberry Pi**:
  - Install Raspberry Pi OS using [Raspberry Pi Imager](https://www.raspberrypi.org/software/).
  - Install Mosquitto:
    ```bash
    sudo apt update
    sudo apt install mosquitto mosquitto-clients
    sudo systemctl enable mosquitto
    ```
  - Install Node-RED:
    ```bash
    bash <(curl -sL https://raw.githubusercontent.com/node-red/linux-installers/master/deb/update-nodejs-and-nodered)
    sudo systemctl enable nodered
    ```
  - Install InfluxDB:
    ```bash
    sudo apt install influxdb
    sudo systemctl enable influxdb
    ```
  - Install Grafana:
    ```bash
    sudo apt install grafana
    sudo systemctl enable grafana-server
    ```

### 3. Programming the ESP32
- Download the source code from the `/src/esp32_environment_monitor.ino` file in this repository.
- Update the following parameters in the code:
  - WiFi: `ssid`, `password`.
  - MQTT: `mqtt_server` (Raspberry Pi IP), `mqtt_port` (1883).
- Upload the code to the ESP32 using Arduino IDE.

### 4. Configuring Node-RED
- Access Node-RED at `http://<Raspberry_Pi_IP>:1880`.
- Install additional nodes: `node-red-contrib-influxdb`, `node-red-node-email`.
- Create a flow to:
  - Receive data from MQTT topics.
  - Store data in InfluxDB.
  - Check thresholds and send alerts via email/Telegram.
- Sample Node-RED flow:
  ```json
  [
    {
      "id": "mqtt_dust",
      "type": "mqtt in",
      "topic": "env/dust",
      "broker": "mqtt_broker",
      "wires": [["threshold_dust", "influxdb_out"]]
    },
    {
      "id": "threshold_dust",
      "type": "function",
      "name": "Check Dust",
      "func": "if (parseFloat(msg.payload) > 150) {\n  msg.payload = 'Alert: Dust level exceeds 150 µg/m³!';\n  return msg;\n}\nreturn null;",
      "wires": [["email_node"]]
    }
  ]
  ```

### 5. Configuring Grafana
- Access Grafana at `http://<Raspberry_Pi_IP>:3000` (default: admin/admin).
- Add an InfluxDB datasource, specifying the `env_data` database.
- Create a dashboard with panels for temperature, humidity, air quality, pressure, and dust levels.

### 6. Sensor Calibration
- **MQ-135**: Place in a clean environment (no smoke, chemicals) for 24 hours; refer to the datasheet to calibrate CO2/VOCs.
- **GP2Y1014AU0F**: Place in a dust-free environment, avoid bright light, and record baseline (0 µg/m³).
- **BMP280**: Compare pressure readings with local weather data.
- **AHT20, DHT11**: Place in a stable room for 24 hours, compare values to verify accuracy.

## Usage Instructions
1. **Start the system**:
   - Connect the ESP32 and Raspberry Pi to power.
   - Ensure both devices are on the same WiFi network.
2. **View data**:
   - Access the Grafana dashboard via a browser (`http://<Raspberry_Pi_IP>:3000`).
   - Monitor charts for temperature, humidity, air quality, pressure, and dust levels.
3. **Receive alerts**:
   - Alerts are sent via email/Telegram when:
     - Temperature > 35°C.
     - Humidity > 80%.
     - Air quality > 1000 ppm.
     - Dust > 150 µg/m³.
4. **Analyze data**:
   - Use the dashboard to analyze trends (e.g., dust spikes during rush hours, pressure drops before rain).
   - Encourage students to log observations and discuss environmental pollution.

## Deployment
- **Location**: School courtyard or public park.
- **Installation**:
   - Place the ESP32 + sensors in a well-ventilated area, avoiding bright light and high humidity (use a waterproof enclosure if needed).
   - Place the Raspberry Pi in a room or area with stable power and WiFi.
- **Education**:
   - Host a presentation for students/teachers (10-20 people).
   - Explain system operation and benefits (pollution monitoring, IoT learning).
   - Guide participants on using the dashboard and analyzing environmental data.

## Notes
- **Security**:
  - Set a password for Mosquitto (`mosquitto_passwd`).
  - Enable HTTPS for Grafana if the dashboard is publicly accessible.
- **Power Supply**:
  - Use a 5V/2A adapter for the ESP32 and Raspberry Pi.
  - Ensure a stable power source to avoid interruptions.
- **Maintenance**:
  - Check sensors biweekly to ensure they are free from moisture or dust buildup.
  - Update code/dashboard as needed to add features.

## Contributing
The project is open to community contributions:
- Add new sensors (e.g., MQ-7 for CO, BH1750 for light).
- Optimize ESP32 code (reduce power consumption).
- Integrate a mobile app (Blynk, MIT App Inventor).
Please submit a pull request or contact via email: [thaivanhoa2002@gmail.com].

## Author
- **Thái Văn Hòa**: Student, programmer, hardware designer, project implementer.
- Implementation Period: May 1, 2025 - June 30, 2025.

## License
This project is licensed under the [MIT License](LICENSE).
