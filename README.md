# Hệ Thống Chấm Công Vân Tay ESP32-S3 + R307 + CMS

Hệ thống chấm công vân tay hoàn chỉnh với tích hợp CMS backend. ESP32-S3 kết nối WiFi, gửi fingerprint template data đến server CMS để xác thực và nhận kết quả access.

## 🌟 Tính Năng Mới

### ✅ Quản lý vân tay đầy đủ
- **Enroll**: Đăng ký vân tay mới (lưu local + sync CMS)
- **Delete**: Xóa vân tay theo ID hoặc xóa tất cả
- **List**: Liệt kê vân tay đã lưu
- **Update**: Cập nhật thông tin vân tay (via CMS)

### ✅ Authentication với CMS
- Gửi **fingerprint template data** (512 bytes) thay vì chỉ ID
- CMS quản lý database vân tay tập trung
- Nhận response `{"access": true/false}` từ CMS
- Gửi kèm **device ID** (MAC address) và **confidence score**

### ✅ Auto-Login Mode
- Tự động scan vân tay mỗi giây
- Tự động gửi request đến CMS khi phát hiện vân tay
- LED feedback:
  - 🟣 Purple: Đang xử lý
  - 🔵 Blue: Access granted
  - 🔴 Red: Access denied

### ✅ WiFi Management
- Kết nối WiFi tự động khi khởi động
- Cấu hình WiFi qua Serial Monitor
- Hỗ trợ offline mode (không cần WiFi)

### ✅ Menu tương tác qua Serial
- Giao diện menu đẹp, dễ sử dụng
- Các lệnh đơn giản (e, d, l, t, a, w, i, h)
- Hiển thị trạng thái WiFi và Auto-Login

## 🔌 Sơ Đồ Kết Nối

### R307 Fingerprint Sensor (UART)
```
R307 Pin           Màu Dây      ->    ESP32-S3 Pin
======================================================
VCC                Đỏ           ->    3.3V hoặc 5V
GND                Đen          ->    GND
TXD                Xanh lá      ->    GPIO16 (RX)
RXD                Trắng        ->    GPIO17 (TX)
WAKEUP             Vàng         ->    Không kết nối
```

## 📦 Cài Đặt

### 1. Clone và cấu hình

```bash
cd esp32s3

# Sửa file config.h
nano src/config.h
```

**Cấu hình WiFi và CMS API:**

```cpp
// src/config.h
#define WIFI_SSID "YourWiFiSSID"
#define WIFI_PASSWORD "YourPassword"

#define CMS_API_URL "http://your-cms.com/api/fingerprint/verify"
#define CMS_ENROLL_URL "http://your-cms.com/api/fingerprint/enroll"
#define CMS_API_KEY "your-api-key-here"  // Optional
```

### 2. Build và Upload

```bash
# Cài thư viện
pio lib install

# Build
pio run

# Upload lên ESP32
pio run --target upload

# Mở Serial Monitor
pio device monitor
```

## 🚀 Sử Dụng

### Menu Điều Khiển

Sau khi khởi động, bạn sẽ thấy menu:

```
╔════════════════════════════════════════╗
║             MENU ĐIỀU KHIỂN           ║
╠════════════════════════════════════════╣
║  [e] Đăng ký vân tay mới              ║
║  [d] Xóa vân tay                      ║
║  [l] Liệt kê vân tay đã lưu           ║
║  [t] Test login với vân tay           ║
║  [a] Bật/Tắt Auto-Login Mode          ║
║  [w] Cấu hình WiFi                    ║
║  [i] Thông tin cảm biến               ║
║  [h] Hiển thị menu này                ║
╚════════════════════════════════════════╝

⚪ AUTO-LOGIN MODE: TẮT
📶 WiFi: CONNECTED (192.168.1.100)

Gõ lệnh:
```

### 1. Đăng ký vân tay mới

```
Gõ: e
→ Nhập ID vân tay (1-127): 1
→ Đặt ngón tay lên cảm biến...
✓ Đã lấy ảnh lần 1
→ Nhấc tay ra...
→ Đặt lại ngón tay lên cảm biến...
✓ Đã lấy ảnh lần 2
✓ Đã lưu vân tay ID #1 thành công!

→ Đang lấy template data...
✓ Template size: 512 bytes

Gửi vân tay lên CMS? (y/n): y
Nhập tên người dùng (hoặc Enter để bỏ qua): Nguyen Van A

✓ Đăng ký vân tay lên CMS thành công!
```

### 2. Test Login

```
Gõ: t
╔════════════════════════════════════════╗
║         TEST LOGIN VỚI VÂN TAY        ║
╚════════════════════════════════════════╝
Đặt ngón tay lên cảm biến...

✓ Tìm thấy vân tay! ID: #1, Độ tin cậy: 142

→ Đang gửi POST request...
URL: http://your-cms.com/api/fingerprint/verify
HTTP Code: 200

╔════════════════════════════════════════╗
║       ✓ ACCESS GRANTED!               ║
╠════════════════════════════════════════╣
║   Fingerprint ID: #1                  ║
║   Confidence: 142                      ║
╚════════════════════════════════════════╝
```

### 3. Auto-Login Mode

```
Gõ: a
🔵 AUTO-LOGIN MODE: ĐÃ BẬT
Hệ thống sẽ tự động scan vân tay mỗi giây.
Gõ 'a' để tắt.

→ Phát hiện vân tay!
→ Đang xác thực với CMS...
✓ ACCESS GRANTED!
[LED xanh sáng 2 giây]
```

### 4. Xóa vân tay

```
Gõ: d
Nhập ID vân tay cần xóa (1-127, hoặc 0 để xóa TẤT CẢ): 1
✓ Đã xóa vân tay ID #1
```

### 5. Cấu hình WiFi

```
Gõ: w
╔════════════════════════════════════════╗
║         CẤU HÌNH WiFi                 ║
╚════════════════════════════════════════╝
Nhập SSID: MyNewWiFi
Nhập Password: mypassword123

→ Đang kết nối WiFi...
✓ KẾT NỐI WiFi THÀNH CÔNG!
IP Address: 192.168.1.100
```

## 🏗️ Kiến Trúc Code

### Module Structure

```
src/
├── main.cpp                    # Main logic + Menu system
├── config.h                    # WiFi & API configuration
│
├── fingerprint-handler.h       # R307 sensor handler
├── fingerprint-handler.cpp     # Enroll, verify, delete, getTemplate
│
├── wifi-manager.h              # WiFi connection manager
├── wifi-manager.cpp            # Connect, disconnect, status
│
├── http-client.h               # HTTP client wrapper
├── http-client.cpp             # POST/GET requests, JSON parse
│
├── auth-manager.h              # CMS authentication
└── auth-manager.cpp            # Verify & enroll with CMS
```

### Data Flow

```
┌─────────────────┐
│  User chấm vân  │
│      tay        │
└────────┬────────┘
         │
         ▼
┌─────────────────────────────┐
│  R307 Sensor                │
│  - Scan fingerprint         │
│  - Return ID + Confidence   │
└────────┬────────────────────┘
         │
         ▼
┌─────────────────────────────┐
│  FingerprintHandler         │
│  - getTemplate(id)          │
│  - Return 512 bytes data    │
└────────┬────────────────────┘
         │
         ▼
┌─────────────────────────────┐
│  AuthManager                │
│  - Encode Base64            │
│  - Create JSON payload:     │
│    {                        │
│      device_id,             │
│      fingerprint_id,        │
│      template_data,         │
│      confidence             │
│    }                        │
└────────┬────────────────────┘
         │
         ▼
┌─────────────────────────────┐
│  HTTPClient                 │
│  - POST to CMS API          │
│  - Timeout: 10s             │
└────────┬────────────────────┘
         │
         ▼
┌─────────────────────────────┐
│  CMS Backend                │
│  - Verify fingerprint       │
│  - Check access permissions │
│  - Return JSON:             │
│    {                        │
│      access: true/false,    │
│      reason: "..."          │
│    }                        │
└────────┬────────────────────┘
         │
         ▼
┌─────────────────────────────┐
│  ESP32-S3                   │
│  - Parse response           │
│  - LED feedback:            │
│    * Blue = Access granted  │
│    * Red = Access denied    │
└─────────────────────────────┘
```

## 📡 API Contract cho CMS Backend

### 1. Verify Fingerprint

**Endpoint:** `POST /api/fingerprint/verify`

**Request:**
```json
{
  "device_id": "AA:BB:CC:DD:EE:FF",
  "fingerprint_id": 1,
  "confidence": 142,
  "template_data": "base64_encoded_512_bytes...",
  "timestamp": 1234567890
}
```

**Response Success (200):**
```json
{
  "access": true,
  "user": {
    "id": 123,
    "name": "Nguyen Van A",
    "role": "employee"
  },
  "timestamp": "2024-01-15T08:30:00Z"
}
```

**Response Denied (200):**
```json
{
  "access": false,
  "reason": "Fingerprint not registered",
  "timestamp": "2024-01-15T08:30:00Z"
}
```

### 2. Enroll Fingerprint

**Endpoint:** `POST /api/fingerprint/enroll`

**Request:**
```json
{
  "device_id": "AA:BB:CC:DD:EE:FF",
  "fingerprint_id": 1,
  "template_data": "base64_encoded_512_bytes...",
  "user_name": "Nguyen Van A",
  "timestamp": 1234567890
}
```

**Response Success (201):**
```json
{
  "success": true,
  "message": "Fingerprint enrolled successfully",
  "user_id": 123
}
```

**Response Error (400):**
```json
{
  "success": false,
  "message": "Fingerprint already exists"
}
```

## 🔧 Thư Viện Được Sử Dụng

```ini
[env:esp32-s3-devkitc-1]
lib_deps =
    adafruit/Adafruit Fingerprint Sensor Library@^2.1.3
    bblanchon/ArduinoJson@^7.2.1
```

## 📝 Lưu Ý Kỹ Thuật

### Fingerprint Template Data
- R307 lưu **fingerprint template**, không phải ảnh gốc
- Template size: **512 bytes** (compressed feature data)
- Được encode **Base64** trước khi gửi qua HTTP (~683 chars)
- CMS cần **decode Base64** để lấy raw template

### R307 Capacity
- Sensor lưu được **1000 vân tay** (không phải 127)
- Adafruit library giới hạn ID từ **1-127**
- Để dùng full 1000 slots, cần implement raw UART protocol

### WiFi Timeout
- Kết nối WiFi timeout: **20 giây**
- HTTP request timeout: **10 giây**
- Auto-reconnect nếu mất kết nối

### Security
- **⚠️ WARNING**: Template data được gửi qua HTTP (không mã hóa)
- Production nên dùng **HTTPS** với SSL/TLS
- Cân nhắc thêm **API signature** để verify request integrity

## 🔮 Tính Năng Tương Lai

- [ ] HTTPS support với SSL certificate
- [ ] NTP time sync cho accurate timestamps
- [ ] SPIFFS/LittleFS để lưu config persistent
- [ ] OTA firmware update
- [ ] Web interface để quản lý qua browser
- [ ] MQTT support cho real-time notifications
- [ ] RTC DS3231 để lưu thời gian offline
- [ ] SD Card logging khi WiFi mất kết nối
- [ ] Multi-language support (EN/VI)

## 🐛 Troubleshooting

### R307 không phản hồi
- Kiểm tra TX/RX đúng chưa (TX→RX, RX→TX)
- Thử đổi GPIO 16↔17
- Chạy test: `./quick-test.sh` (option 2)
- Xem [docs/TROUBLESHOOTING-ESP32S3.md](docs/TROUBLESHOOTING-ESP32S3.md)

### WiFi không kết nối được
- Kiểm tra SSID/Password trong `config.h`
- Dùng lệnh `w` để thử WiFi khác
- ESP32-S3 chỉ hỗ trợ **2.4GHz WiFi** (không hỗ trợ 5GHz)

### CMS trả về error 401/403
- Kiểm tra `CMS_API_KEY` trong `config.h`
- Xem log HTTP response trên Serial Monitor
- Verify API endpoint URL đúng chưa

### Template data bị lỗi
- Adafruit library có giới hạn trong việc download template
- Check `finger->fingerTemplate[]` array size
- Có thể cần implement raw UART communication

## 📄 License

MIT License

## 👨‍💻 Tác Giả

ESP32-S3 IoT Project - R307 Fingerprint + CMS Integration
