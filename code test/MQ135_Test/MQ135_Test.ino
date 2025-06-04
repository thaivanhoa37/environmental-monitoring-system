/*
 * Code test cảm biến MQ135 đo chất lượng không khí
 * Tác giả: Cline Assistant
 * Ngày: 26/05/2025
 * 
 * Kết nối phần cứng:
 * MQ135 -> Arduino UNO
 * - VCC  -> 5V
 * - GND  -> GND  
 * - AOUT -> A0   (Đọc giá trị analog)
 * - DOUT -> D2   (Đọc giá trị digital/ngưỡng)
 */

// Định nghĩa các chân kết nối
const int AOUT_PIN = A0;    // Chân đọc giá trị analog từ cảm biến
const int DOUT_PIN = 2;     // Chân đọc giá trị digital từ cảm biến

// Các biến lưu giá trị
int giaTriAnalog = 0;       // Lưu giá trị analog đọc được (0-1023)
int giaTriDigital = 0;      // Lưu giá trị digital đọc được (0/1)

void setup() {
  // Khởi tạo giao tiếp Serial với tốc độ 9600 baud
  Serial.begin(9600);
  
  // Cấu hình chân digital là đầu vào
  pinMode(DOUT_PIN, INPUT);
  
  // In thông báo khởi động
  Serial.println(F("Khởi động cảm biến MQ135..."));
  Serial.println(F("Vui lòng đợi 20 giây để cảm biến ổn định..."));
  
  // Đợi cảm biến khởi động và ổn định (khoảng 20 giây)
  delay(20000);
  
  // In thông báo sẵn sàng
  Serial.println(F("Cảm biến đã sẵn sàng!"));
  Serial.println(F("Bắt đầu đọc dữ liệu...\n"));
}

void loop() {
  // Đọc giá trị analog từ cảm biến
  giaTriAnalog = analogRead(AOUT_PIN);
  
  // Đọc giá trị digital từ cảm biến
  giaTriDigital = digitalRead(DOUT_PIN);
  
  // In giá trị analog
  Serial.print(F("Giá trị Analog: "));
  Serial.print(giaTriAnalog);
  Serial.print(F("\t"));
  
  // In giá trị digital
  Serial.print(F("Giá trị Digital: "));
  Serial.print(giaTriDigital);
  
  // In đánh giá dựa trên giá trị analog
  Serial.print(F("\tĐánh giá: "));
  if (giaTriAnalog < 100) {
    Serial.println(F("Không khí sạch"));
  }
  else if (giaTriAnalog < 300) {
    Serial.println(F("Chất lượng không khí tốt"));
  }
  else if (giaTriAnalog < 500) {
    Serial.println(F("Chất lượng không khí trung bình"));
  }
  else {
    Serial.println(F("Chất lượng không khí kém!"));
  }
  
  // Đợi 1 giây trước khi đọc giá trị tiếp theo
  delay(1000);
}
