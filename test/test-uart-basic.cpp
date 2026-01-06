/*
 * TEST CƠ BẢN CHO R307 TRÊN ESP32-S3
 * Mục đích: Kiểm tra kết nối UART đơn giản
 * Dùng khi: Mạch chớp rồi tắt, không kết nối được
 */

#include <Arduino.h>
#include <Adafruit_Fingerprint.h>

// Cấu hình UART - THỬ CẢ 2 CÁCH
// Cách 1: GPIO16/17 (mặc định)
#define R307_RX 16
#define R307_TX 17

// Cách 2: Nếu cách 1 không được, đổi thành GPIO9/10
// #define R307_RX 9
// #define R307_TX 10

HardwareSerial mySerial(2); // UART2
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n╔═══════════════════════════════════╗");
  Serial.println("║  TEST UART ESP32-S3 + R307        ║");
  Serial.println("╚═══════════════════════════════════╝\n");

  // Hiển thị cấu hình
  Serial.printf("→ RX Pin: GPIO%d\n", R307_RX);
  Serial.printf("→ TX Pin: GPIO%d\n", R307_TX);
  Serial.println("→ Baud: 57600");
  Serial.println();

  // Khởi động UART với nhiều delay
  Serial.println("→ Khởi động UART...");
  mySerial.begin(57600, SERIAL_8N1, R307_RX, R307_TX);
  delay(1000); // Delay dài cho ESP32-S3

  // Flush buffer
  while (mySerial.available()) {
    mySerial.read();
  }
  Serial.println("✓ UART đã khởi động");
  Serial.println();

  // Khởi động fingerprint library
  Serial.println("→ Khởi động thư viện Adafruit...");
  finger.begin(57600);
  delay(500);

  // Flush lại buffer
  while (mySerial.available()) {
    mySerial.read();
  }
  Serial.println("✓ Thư viện đã khởi động");
  Serial.println();

  // Test verify password với nhiều retry
  Serial.println("→ Test kết nối với R307...");
  Serial.println("  (Đèn R307 phải sáng liên tục)");
  Serial.println();

  bool success = false;
  for (int i = 1; i <= 5; i++)
  {
    Serial.printf("→ Thử lần %d/5... ", i);

    if (finger.verifyPassword())
    {
      Serial.println("✓ THÀNH CÔNG!");
      success = true;
      break;
    }
    else
    {
      Serial.println("✗ Thất bại");
      delay(1000);
    }
  }

  Serial.println();

  if (success)
  {
    // THÀNH CÔNG - Hiển thị thông tin
    Serial.println("╔═══════════════════════════════════╗");
    Serial.println("║     ✓ KẾT NỐI THÀNH CÔNG!        ║");
    Serial.println("╚═══════════════════════════════════╝");
    Serial.println();

    Serial.println("→ Thông tin cảm biến:");
    Serial.printf("  - Password: 0x%08X\n", finger.password);

    finger.getParameters();
    Serial.printf("  - Dung lượng: %d templates\n", finger.capacity);
    Serial.printf("  - Baud rate: 0x%X\n", finger.baud_rate);
    Serial.printf("  - Packet len: %d bytes\n", finger.packet_len);

    // Test LED
    Serial.println();
    Serial.println("→ Test LED...");
    Serial.println("  - Đỏ (2s)");
    finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_RED);
    delay(2000);

    Serial.println("  - Xanh (2s)");
    finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
    delay(2000);

    Serial.println("  - Tím (2s)");
    finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);
    delay(2000);

    finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_BLUE);
    Serial.println("✓ Test LED hoàn tất!");
  }
  else
  {
    // THẤT BẠI - Hiển thị hướng dẫn
    Serial.println("╔═══════════════════════════════════╗");
    Serial.println("║     ✗ KẾT NỐI THẤT BẠI!          ║");
    Serial.println("╚═══════════════════════════════════╝");
    Serial.println();

    Serial.println("KIỂM TRA LẠI:");
    Serial.println();
    Serial.println("1. ĐÈN R307:");
    Serial.println("   ✓ Sáng liên tục → OK");
    Serial.println("   ✗ Chớp rồi tắt → Lỗi nguồn!");
    Serial.println("   ✗ Không sáng → Không có nguồn");
    Serial.println();
    Serial.println("2. ĐẤU DÂY:");
    Serial.printf("   - R307 VCC (Đỏ)    → 3.3V hoặc 5V\n");
    Serial.printf("   - R307 GND (Đen)   → GND\n");
    Serial.printf("   - R307 TX (Xanh)   → GPIO%d\n", R307_RX);
    Serial.printf("   - R307 RX (Trắng)  → GPIO%d\n", R307_TX);
    Serial.println();
    Serial.println("3. THỬ ĐỔI TX/RX:");
    Serial.println("   - Mở file và đổi:");
    Serial.println("     #define R307_RX 17  (thay vì 16)");
    Serial.println("     #define R307_TX 16  (thay vì 17)");
    Serial.println();
    Serial.println("4. THỬ GPIO KHÁC:");
    Serial.println("   - Thử GPIO9/10:");
    Serial.println("     #define R307_RX 9");
    Serial.println("     #define R307_TX 10");
  }
}

void loop()
{
  // Không làm gì trong loop
  delay(1000);
}
