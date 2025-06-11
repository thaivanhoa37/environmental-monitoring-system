# HƯỚNG DẪN KẾT NỐI CẢM BIẾN VỚI ESP32

## 1. CẢNG KẾT NỐI

### DHT11 -> ESP32:
- VCC  -> 3.3V
- GND  -> GND
- DATA -> GPIO4 (D4)

### AHT20 -> ESP32:
- VCC  -> 3.3V
- GND  -> GND
- SDA  -> GPIO21 (SDA)
- SCL  -> GPIO22 (SCL)

### BMP280 -> ESP32:
- VCC  -> 3.3V
- GND  -> GND
- SDA  -> GPIO21 (SDA)
- SCL  -> GPIO22 (SCL)
- CSB  -> 3.3V
- SDO  -> GND

### MQ135 -> ESP32:
- VCC  -> 5V      (Quan trọng: cần nguồn 5V)
- GND  -> GND
- AOUT -> GPIO35  (ADC1_CH7)

### OLED Display -> ESP32:
- VCC  -> 3.3V
- GND  -> GND
- SDA  -> GPIO21 (SDA)
- SCL  -> GPIO22 (SCL)

## 2. LƯU Ý QUAN TRỌNG

- MQ135 cần nguồn 5V để hoạt động chính xác
- I2C devices (AHT20, BMP280, OLED) dùng chung bus SDA/SCL
- Sử dụng ADC1 cho MQ135 vì ADC2 không hoạt động khi WiFi bật
- DHT11 cần điện trở pull-up 10kΩ giữa DATA và VCC

## 3. KIỂM TRA KẾT NỐI

- Serial monitor sẽ hiển thị quá trình khởi động của từng cảm biến
- Chương trình sẽ quét địa chỉ I2C khi khởi động
- Thời gian khởi động MQ135: 20 giây để ổn định
- OLED sẽ hiển thị trạng thái khởi động

## 4. XỬ LÝ LỖI THƯỜNG GẶP

- Nếu DHT11 không đọc được: Kiểm tra điện trở pull-up
- Nếu MQ135 không đọc được: Đảm bảo cấp nguồn 5V
- Nếu I2C devices không hoạt động: Kiểm tra địa chỉ và kết nối SDA/SCL
- Nếu giá trị bất thường: Kiểm tra điện áp nguồn và ground

## 5. THÔNG SỐ KỸ THUẬT

- Điện áp hoạt động: 3.3V (DHT11, AHT20, BMP280, OLED), 5V (MQ135)
- Dải đo:
  + Nhiệt độ: 0-50°C (DHT11), -40-85°C (AHT20)
  + Độ ẩm: 20-90% (DHT11), 0-100% (AHT20)
  + Áp suất: 300-1100 hPa (BMP280)
  + CO2: 400-5000 ppm (MQ135)

## 6. TIÊU CHUẨN CHẤT LƯỢNG KHÔNG KHÍ (AQI)

Theo tiêu chuẩn của Tổ chức Y tế Thế giới (WHO) và EPA (Mỹ), chỉ số PM2.5 được phân loại như sau:

| PM2.5 (µg/m³) | Chỉ số AQI | Mức độ        | Màu sắc |
|---------------|------------|---------------|---------|
| 0-12          | 0-50       | Tốt          | Xanh lá |
| 12.1-35.4     | 51-100     | Trung bình   | Vàng    |
| 35.5-55.4     | 101-150    | Không tốt cho nhóm nhạy cảm | Cam     |
| 55.5-150.4    | 151-200    | Xấu          | Đỏ      |
| 150.5-250.4   | 201-300    | Rất xấu      | Tím     |
| 250.5-500.4   | 301-500    | Nguy hiểm    | Nâu đỏ  |

## 7. SƠ ĐỒ KẾT NỐI
```
                                 ESP32 DevKit
                              ┌──────────────┐
                              │   USB Port   │
                              └──────────────┘
                              ┌──────────────┐
       ┌── DHT11 (D4) ───────┤              │
       │                     │              │
       ├── MQ135 (GPIO35) ──┤     ESP32    │
       │                     │              │
       │    ┌─── AHT20 ─────┤   SDA(21)    │
       │    │               │   SCL(22)    │
I2C ───┼────┼─── BMP280 ───┤              │
Bus    │    │               │     3.3V     │
       │    └─── OLED ─────┤     GND      │
       │                    │              │
       │                    │      5V      │
       │                    └──────────────┘
       │                          │
       └──────────────────────────┘
```

### Chú thích sơ đồ:
• Các thiết bị I2C (AHT20, BMP280, OLED) chia sẻ chung đường SDA/SCL
• MQ135 cần nguồn 5V riêng
• DHT11 cần điện trở pull-up 10kΩ
