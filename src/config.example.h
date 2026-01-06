#ifndef CONFIG_H
#define CONFIG_H

// ==========================================
// WiFi Configuration
// ==========================================
// Thay bằng WiFi credentials của bạn
#define WIFI_SSID "YourWiFiSSID"        // Ví dụ: "MyHomeWiFi"
#define WIFI_PASSWORD "YourPassword"     // Ví dụ: "mypassword123"
#define WIFI_TIMEOUT_MS 20000            // Timeout kết nối WiFi (20 giây)

// ==========================================
// Directus API Configuration
// ==========================================
// Thay bằng Directus URL và Static Token của bạn
#define DIRECTUS_URL "https://your-directus.com"  // Directus URL (VD: https://cms.company.com)
#define DIRECTUS_TOKEN "your-static-token-here"   // Static Token từ Directus Settings → Access Tokens

// ==========================================
// R307 Fingerprint Sensor Configuration
// ==========================================
#define R307_RX_PIN 16                   // ESP32 GPIO16 (RX) -> R307 TX (Xanh lá)
#define R307_TX_PIN 17                   // ESP32 GPIO17 (TX) -> R307 RX (Trắng)
#define R307_BAUD_RATE 57600             // Baud rate của R307 (cố định 57600)

// ==========================================
// Device Configuration
// ==========================================
// Device ID sẽ tự động dùng MAC address của ESP32
// Không cần config gì thêm

// ==========================================
// Timing Configuration
// ==========================================
#define FINGERPRINT_CHECK_INTERVAL 1000  // Kiểm tra vân tay mỗi 1 giây (auto-login mode)
#define HTTP_TIMEOUT_MS 10000            // HTTP request timeout (10 giây)

// ==========================================
// NOTES
// ==========================================
// 1. Copy file này thành "config.h" và sửa các giá trị trên
// 2. KHÔNG commit file config.h lên git (đã có trong .gitignore)
// 3. Nếu không dùng API key, để trống: #define CMS_API_KEY ""

#endif
