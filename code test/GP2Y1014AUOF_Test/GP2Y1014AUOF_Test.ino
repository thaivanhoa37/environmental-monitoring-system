/*
 * Test cảm biến bụi GP2Y1014AUOF
 * Kết nối:
 * V-LED -> 3.3V (qua điện trở 150Ω và tụ 220μF)
 * LED-GND -> GND
 * LED -> D2
 * S-GND -> GND
 * Vo -> A0
 * Vcc -> 5V
 */

// Định nghĩa chân kết nối
const int ledPin = 2;        // Chân điều khiển LED
const int measurePin = A0;   // Chân đọc tín hiệu analog

// Các thông số thời gian theo datasheet
const int samplingTime = 280;  // Thời gian lấy mẫu
const int deltaTime = 40;      // Thời gian giữa tắt LED và đo
const int sleepTime = 9680;    // Thời gian nghỉ

// Các thông số cảm biến và bộ lọc
float voMeasured = 0;      // Điện áp đo được
float calcVoltage = 0;     // Điện áp đã tính toán
float dustDensity = 0;     // Mật độ bụi
float filterWeight = 0.1;  // Hệ số lọc (0.0-1.0)
float filteredDensity = 0; // Mật độ bụi sau khi lọc

// Ngưỡng đánh giá chất lượng không khí (mg/m3)
const float EXCELLENT = 0.012;  // Rất tốt
const float GOOD = 0.035;      // Tốt
const float MODERATE = 0.055;   // Trung bình
const float POOR = 0.150;      // Kém
const float VERY_POOR = 0.250; // Rất kém

String getAirQuality(float density) {
  if (density <= EXCELLENT) return "Rat tot";
  if (density <= GOOD) return "Tot";
  if (density <= MODERATE) return "Trung binh";
  if (density <= POOR) return "Kem";
  if (density <= VERY_POOR) return "Rat kem";
  return "Nguy hai";
}

void setup() {
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT);
  
  Serial.println("Khoi dong cam bien bui GP2Y1014AUOF...");
  Serial.println("=====================================");
  Serial.println("Thang danh gia chat luong khong khi:");
  Serial.println("< 0.012 mg/m3: Rat tot");
  Serial.println("< 0.035 mg/m3: Tot");
  Serial.println("< 0.055 mg/m3: Trung binh");
  Serial.println("< 0.150 mg/m3: Kem");
  Serial.println("< 0.250 mg/m3: Rat kem");
  Serial.println(">= 0.250 mg/m3: Nguy hai");
  Serial.println("=====================================");
  Serial.println("Cho 3-5 phut de cam bien on dinh...");
  delay(2000);
}

void loop() {
  // Đọc giá trị cảm biến
  digitalWrite(ledPin, LOW);  // Bật LED
  delayMicroseconds(samplingTime);
  
  // Lấy trung bình của 5 lần đọc để giảm nhiễu
  voMeasured = 0;
  for(int i = 0; i < 5; i++) {
    voMeasured += analogRead(measurePin);
    delayMicroseconds(50);
  }
  voMeasured = voMeasured / 5.0;
  
  delayMicroseconds(deltaTime);
  digitalWrite(ledPin, HIGH);  // Tắt LED
  delayMicroseconds(sleepTime);

  // Chuyển đổi điện áp
  calcVoltage = voMeasured * (5.0 / 1024);
  
  // Tính toán mật độ bụi theo công thức từ datasheet
  dustDensity = 0.17 * calcVoltage - 0.1;
  
  if (dustDensity < 0) {
    dustDensity = 0;
  }

  // Áp dụng bộ lọc để làm mịn giá trị
  filteredDensity = (filterWeight * dustDensity) + ((1 - filterWeight) * filteredDensity);

  // In kết quả
  Serial.print("ADC: ");
  Serial.print(voMeasured, 0);
  Serial.print(" | Density: ");
  Serial.print(filteredDensity, 3);
  Serial.print(" mg/m3");
  Serial.print(" | KQ: ");
  Serial.print(getAirQuality(filteredDensity));

  // Cảnh báo nếu giá trị bất thường
  if (filteredDensity > 1.0) {
    Serial.print(" | CANH BAO: Gia tri qua cao, kiem tra lai cam bien!");
  }
  Serial.println();

  delay(1000);
}
