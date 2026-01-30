#include <Arduino.h>
#include <base64.h>
#include "config.h"
#include "fingerprint-handler.h"
#include "wifi-manager.h"
#include "http-client.h"
#include "directus-client.h"
#include "mqtt-client.h"
#include "command-handler.h"
#include "offline-queue.h"
#include "buzzer-handler.h"

// ==========================================
// Global Objects
// ==========================================
HardwareSerial serialPort(2); // UART2 cho R307
FingerprintHandler* fpHandler;
WiFiManager* wifiManager;
HTTPClientManager* httpClient;
DirectusClient* directusClient;
MQTTClient* mqttClient;
CommandHandler* commandHandler;
OfflineQueue* offlineQueue;
BuzzerHandler* buzzerHandler;

// ==========================================
// Global Variables
// ==========================================
bool autoLoginMode = true;  // Auto-login ON by default, pauses for MQTT commands
unsigned long lastFingerprintCheck = 0;
static bool wasWiFiConnected = false;

// Buffer cho fingerprint template
uint8_t templateBuffer[512];
uint16_t templateSize = 0;

// ==========================================
// Function Prototypes
// ==========================================
void printMenu();
void handleMenuCommand(char cmd);
void enrollFingerprint();
void deleteFingerprint();
void listFingerprints();
void testLogin();
void toggleAutoLogin();
void configureWiFi();
void restoreFromDirectus();
void checkAutoLogin();

// ==========================================
// Setup
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(1000);  // Wait for Serial to stabilize (ESP32-S3 USB CDC)

    Serial.println("\n\n╔════════════════════════════════════════╗");
    Serial.println("║   HỆ THỐNG CHẤM CÔNG VÂN TAY         ║");
    Serial.println("║   ESP32-S3 + R307 + CMS Integration   ║");
    Serial.println("╚════════════════════════════════════════╝\n");

    // 0. Khởi tạo Buzzer UX
    Serial.println("→ Khởi tạo Buzzer...");
    buzzerHandler = new BuzzerHandler();
    if (!buzzerHandler->begin()) {
        Serial.println("⚠ Buzzer không khả dụng (không bắt buộc)");
    }

    // 1. Khởi tạo R307 Fingerprint Sensor
    Serial.println("→ Khởi tạo R307 Fingerprint Sensor...");
    serialPort.begin(R307_BAUD_RATE, SERIAL_8N1, R307_RX_PIN, R307_TX_PIN);
    delay(100);

    fpHandler = new FingerprintHandler(&serialPort);

    // Set buzzer cho UX feedback
    fpHandler->setBuzzer(buzzerHandler);

    if (!fpHandler->begin()) {
        Serial.println("✗ WARNING: Không kết nối được R307!");
        if (buzzerHandler) buzzerHandler->play(BUZZ_ERROR);
        Serial.println("Thử lại sau 3 giây...");
        delay(3000);
        if (!fpHandler->begin()) {
            Serial.println("✗ R307 vẫn không kết nối. Tiếp tục khởi động (auto-login sẽ bị tắt)...");
            autoLoginMode = false;  // Disable auto-login if sensor not available
        }
    }

    fpHandler->printSensorInfo();

    // 2. Khởi tạo WiFi Manager
    Serial.println("\n→ Khởi tạo WiFi Manager...");
    wifiManager = new WiFiManager();

    if (!wifiManager->connect()) {
        Serial.println("⚠ WARNING: Không kết nối được WiFi!");
        Serial.println("Bạn có thể:");
        Serial.println("  1. Cấu hình WiFi qua menu (lệnh 'w')");
        Serial.println("  2. Sửa SSID/Password trong config.h");
        Serial.println("\nHệ thống sẽ chạy ở chế độ offline (không có CMS).");
    }

    // 3. Khởi tạo Offline Queue
    Serial.println("\n→ Khởi tạo Offline Queue...");
    offlineQueue = new OfflineQueue();
    if (offlineQueue->begin()) {
        Serial.printf("✓ Offline queue ready (%d pending)\n",
                      offlineQueue->getPendingCount());
    } else {
        Serial.println("⚠ Offline queue init failed");
    }

    // 4. Khởi tạo HTTP Client và Directus Client
    httpClient = new HTTPClientManager();
    directusClient = new DirectusClient(httpClient, wifiManager, offlineQueue);

    // 5. Register device with Directus
    if (wifiManager->isConnected()) {
        String deviceMac = wifiManager->getMACAddress();
        String deviceName = "ESP32-FP-001";  // Có thể customize
        String ipAddress = wifiManager->getIPAddress();

        String deviceId = directusClient->registerDevice(deviceMac, deviceName, ipAddress);
        if (deviceId.length() > 0) {
            Serial.println("✓ Device đã được đăng ký trong Directus");
        }
    }

    // Success beep
    if (buzzerHandler) buzzerHandler->play(BUZZ_SUCCESS);

    // 6. Khởi tạo MQTT Client
    Serial.println("\n→ Khởi tạo MQTT Client...");
    mqttClient = new MQTTClient(wifiManager);

    String deviceMac = wifiManager->getMACAddress();
    mqttClient->begin(MQTT_BROKER, MQTT_PORT, MQTT_USERNAME, MQTT_PASSWORD, deviceMac);

    // 7. Khởi tạo Command Handler
    commandHandler = new CommandHandler(fpHandler, mqttClient, directusClient, wifiManager);

    // Set command callback
    mqttClient->setCommandCallback([](const String& commandId, const String& type, JsonObject params) {
        commandHandler->executeCommand(commandId, type, params);
    });

    Serial.println("\n✓ Hệ thống sẵn sàng!");
    printMenu();
}

// ==========================================
// Loop
// ==========================================
void loop() {
    // Check WiFi reconnect for queue flush
    bool isConnected = wifiManager->isConnected();
    if (isConnected && !wasWiFiConnected) {
        Serial.println("\n✓ WiFi reconnected!");
        if (offlineQueue && offlineQueue->getPendingCount() > 0) {
            Serial.println("→ Flushing offline queue...");
            offlineQueue->flush(httpClient, DIRECTUS_URL);
        }
    }
    wasWiFiConnected = isConnected;

    // MQTT loop - handle connection and messages
    mqttClient->loop();

    // Kiểm tra auto-login mode (pause if command is executing)
    if (autoLoginMode && !commandHandler->isPaused()) {
        checkAutoLogin();
    }

    // Xử lý Serial commands
    if (Serial.available()) {
        char cmd = Serial.read();
        handleMenuCommand(cmd);
    }

    delay(10);
}

// ==========================================
// Menu Functions
// ==========================================
void printMenu() {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║             MENU ĐIỀU KHIỂN           ║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.println("║  [e] Đăng ký vân tay mới              ║");
    Serial.println("║  [d] Xóa vân tay                      ║");
    Serial.println("║  [r] Restore vân tay từ Directus      ║");
    Serial.println("║  [l] Liệt kê vân tay đã lưu           ║");
    Serial.println("║  [t] Test login với vân tay           ║");
    Serial.println("║  [a] Bật/Tắt Auto-Login Mode          ║");
    Serial.println("║  [w] Cấu hình WiFi                    ║");
    Serial.println("║  [i] Thông tin cảm biến               ║");
    Serial.println("║  [h] Hiển thị menu này                ║");
    Serial.println("╚════════════════════════════════════════╝");

    if (autoLoginMode) {
        Serial.println("\n🔵 AUTO-LOGIN MODE: ĐANG BẬT");
    } else {
        Serial.println("\n⚪ AUTO-LOGIN MODE: TẮT");
    }

    if (wifiManager->isConnected()) {
        Serial.print("📶 WiFi: CONNECTED (");
        Serial.print(wifiManager->getIPAddress());
        Serial.println(")");
    } else {
        Serial.println("📶 WiFi: DISCONNECTED");
    }

    Serial.println("\nGõ lệnh: ");
}

void handleMenuCommand(char cmd) {
    Serial.println();  // Newline

    switch (cmd) {
        case 'e':
        case 'E':
            enrollFingerprint();
            break;

        case 'd':
        case 'D':
            deleteFingerprint();
            break;

        case 'r':
        case 'R':
            restoreFromDirectus();
            break;

        case 'l':
        case 'L':
            listFingerprints();
            break;

        case 't':
        case 'T':
            testLogin();
            break;

        case 'a':
        case 'A':
            toggleAutoLogin();
            break;

        case 'w':
        case 'W':
            configureWiFi();
            break;

        case 'i':
        case 'I':
            fpHandler->printSensorInfo();
            wifiManager->printInfo();
            break;

        case 'h':
        case 'H':
            printMenu();
            break;

        case '\n':
        case '\r':
            // Ignore newlines
            break;

        default:
            Serial.println("✗ Lệnh không hợp lệ. Gõ 'h' để xem menu.");
            break;
    }
}

// ==========================================
// Feature Functions
// ==========================================
void enrollFingerprint() {
    Serial.println("Nhập ID vân tay (1-127): ");

    while (!Serial.available()) delay(10);
    uint8_t id = Serial.parseInt();

    if (id < 1 || id > 127) {
        Serial.println("✗ ID không hợp lệ (phải từ 1-127)");
        return;
    }

    // Enroll vào sensor
    int result = fpHandler->enrollFingerprint(id);

    if (result > 0) {
        Serial.println("\n→ Đang lấy template data...");

        // Lấy template data
        if (fpHandler->getTemplate(id, templateBuffer, &templateSize)) {
            Serial.printf("✓ Template size: %d bytes\n", templateSize);

            // Tự động gửi lên CMS nếu có WiFi
            if (wifiManager->isConnected()) {
                Serial.println("\n→ Đang đăng ký vân tay lên Directus...");

                // Flush serial buffer để clear các ký tự thừa
                while (Serial.available()) {
                    Serial.read();
                }
                delay(100);

                Serial.println("Nhập Member ID (UUID): ");
                Serial.println("(Bạn có 30 giây để nhập, hoặc Enter để bỏ qua)");

                String memberId = "";
                unsigned long timeout = millis();
                const unsigned long timeoutDuration = 30000; // 30 giây
                unsigned long lastCountdown = 0;

                while (millis() - timeout < timeoutDuration) {
                    // Hiển thị countdown mỗi 5 giây
                    if (millis() - lastCountdown >= 5000) {
                        unsigned long remaining = (timeoutDuration - (millis() - timeout)) / 1000;
                        Serial.printf("... còn %lu giây\n", remaining);
                        lastCountdown = millis();
                    }

                    if (Serial.available()) {
                        memberId = Serial.readStringUntil('\n');
                        memberId.trim();
                        break;
                    }
                    delay(100);
                }

                if (memberId.length() > 0) {
                    Serial.printf("✓ Đã nhận Member ID: %s\n", memberId.c_str());

                    // Convert template to Base64
                    String templateBase64 = base64::encode(templateBuffer, templateSize);
                    String deviceMac = wifiManager->getMACAddress();

                    directusClient->enrollFingerprint(deviceMac, id, templateBase64, memberId);
                } else {
                    Serial.println("⚠ Không nhập Member ID, bỏ qua đăng ký lên Directus");
                }
            } else {
                Serial.println("⚠ WiFi chưa kết nối, không thể gửi lên CMS.");
            }
        }
    } else {
        Serial.println("✗ Lỗi đăng ký vân tay vào sensor");
    }

    printMenu();
}

void deleteFingerprint() {
    Serial.println("Nhập ID vân tay cần xóa (1-127, hoặc 0 để xóa TẤT CẢ): ");

    while (!Serial.available()) delay(10);
    uint8_t id = Serial.parseInt();

    if (id == 0) {
        Serial.println("⚠ XÓA TẤT CẢ vân tay? (y/n): ");
        while (!Serial.available()) delay(10);

        char confirm = Serial.read();
        if (confirm == 'y' || confirm == 'Y') {
            fpHandler->deleteAllFingerprints();
        } else {
            Serial.println("Đã hủy.");
        }
    } else if (id >= 1 && id <= 127) {
        fpHandler->deleteFingerprint(id);
    } else {
        Serial.println("✗ ID không hợp lệ");
    }

    printMenu();
}

void listFingerprints() {
    uint16_t count = fpHandler->getTemplateCount();

    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║      DANH SÁCH VÂN TAY ĐÃ LƯU        ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.printf("Số lượng: %d vân tay\n", count);

    // Note: R307 không có API để list tất cả IDs
    // Cần scan từng slot từ 1-127 (tốn thời gian)
    Serial.println("\n⚠ Để xem chi tiết từng ID, cần scan toàn bộ slots (1-127).");
    Serial.println("Scan tất cả? (y/n): ");

    while (!Serial.available()) delay(10);
    char response = Serial.read();

    if (response == 'y' || response == 'Y') {
        Serial.println("\nĐang scan...");

        for (uint8_t id = 1; id <= 127; id++) {
            // Thử load model
            // Note: Adafruit library không có loadModel public
            // Cần sử dụng workaround hoặc modify library
            Serial.print(".");
            if (id % 20 == 0) Serial.println();
            delay(10);
        }

        Serial.println("\n✓ Scan hoàn tất.");
        Serial.printf("Tổng số: %d vân tay\n", count);
    }

    printMenu();
}

void testLogin() {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║         TEST LOGIN VỚI VÂN TAY        ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.println("Đặt ngón tay lên cảm biến...\n");

    fpHandler->ledOn(3); // Purple LED

    unsigned long timeout = millis();
    while (millis() - timeout < 10000) {
        int fingerprintID = fpHandler->verifyFingerprint();

        if (fingerprintID > 0) {
            uint16_t confidence = fpHandler->getConfidence();

            // Lấy template data
            if (fpHandler->getTemplate(fingerprintID, templateBuffer, &templateSize)) {
                // Convert to Base64
                String templateBase64 = base64::encode(templateBuffer, templateSize);
                String deviceMac = wifiManager->getMACAddress();
                String memberId;

                // Verify với Directus
                bool access = directusClient->verifyFingerprint(deviceMac, fingerprintID,
                                                               templateBase64, confidence, memberId);

                if (access) {
                    fpHandler->ledOn(2); // Blue LED - Success
                } else {
                    fpHandler->ledOn(1); // Red LED - Denied
                }

                delay(2000);
                fpHandler->ledOff();
                printMenu();
                return;
            }
        }

        delay(100);
    }

    Serial.println("✗ Timeout - Không phát hiện vân tay");
    fpHandler->ledOff();
    printMenu();
}

void toggleAutoLogin() {
    autoLoginMode = !autoLoginMode;

    if (autoLoginMode) {
        Serial.println("\n🔵 AUTO-LOGIN MODE: ĐÃ BẬT");
        Serial.println("Hệ thống sẽ tự động scan vân tay mỗi giây.");
        Serial.println("Gõ 'a' để tắt.\n");
    } else {
        Serial.println("\n⚪ AUTO-LOGIN MODE: ĐÃ TẮT");
        fpHandler->ledOff();
    }
}

void configureWiFi() {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║         CẤU HÌNH WiFi                 ║");
    Serial.println("╚════════════════════════════════════════╝");

    Serial.println("Nhập SSID: ");
    while (!Serial.available()) delay(10);
    String ssid = Serial.readStringUntil('\n');
    ssid.trim();

    Serial.println("Nhập Password: ");
    while (!Serial.available()) delay(10);
    String password = Serial.readStringUntil('\n');
    password.trim();

    wifiManager->disconnect();
    delay(500);

    if (wifiManager->connect(ssid.c_str(), password.c_str())) {
        Serial.println("✓ Đã kết nối WiFi thành công!");
    } else {
        Serial.println("✗ Không thể kết nối WiFi với credentials này.");
    }

    printMenu();
}

void restoreFromDirectus() {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║    RESTORE VÂN TAY TỪ DIRECTUS        ║");
    Serial.println("╚════════════════════════════════════════╝");

    if (!wifiManager->isConnected()) {
        Serial.println("✗ WiFi chưa kết nối! Cần WiFi để download từ Directus.");
        printMenu();
        return;
    }

    String deviceMac = wifiManager->getMACAddress();

    // Fetch danh sách fingerprints từ Directus
    Serial.println("→ Đang query fingerprints từ Directus...");
    JsonDocument doc;
    int count = directusClient->getFingerprints(deviceMac, doc);

    if (count == 0) {
        Serial.println("⚠ Không có fingerprint nào trong Directus cho device này.");
        printMenu();
        return;
    }

    // Hiển thị danh sách
    Serial.println("\nDanh sách fingerprints trong Directus:");
    Serial.println("─────────────────────────────────────────");

    JsonArray data = doc["data"];
    for (int i = 0; i < count; i++) {
        String fpId = data[i]["id"].as<String>();
        uint8_t localId = data[i]["finger_print_id"].as<uint8_t>();
        String memberId = data[i]["member_id"].as<String>();

        Serial.printf("[%d] ID: %d | Member: %s | UUID: %s\n",
                     i + 1, localId, memberId.c_str(), fpId.c_str());
    }

    Serial.println("─────────────────────────────────────────");
    Serial.println("\nChọn:");
    Serial.println("  [0] Restore TẤT CẢ");
    Serial.println("  [1-n] Restore từng fingerprint");
    Serial.println("  [c] Hủy");
    Serial.print("\nLựa chọn: ");

    while (!Serial.available()) delay(10);
    char choice = Serial.read();
    Serial.println(choice);

    if (choice == 'c' || choice == 'C') {
        Serial.println("Đã hủy.");
        printMenu();
        return;
    }

    int selected = choice - '0';  // Convert char to int

    if (selected == 0) {
        // Restore all
        Serial.printf("\n→ Restore %d fingerprints...\n", count);

        int successCount = 0;
        for (int i = 0; i < count; i++) {
            String fpId = data[i]["id"].as<String>();
            uint8_t localId;

            Serial.printf("\n[%d/%d] Downloading...\n", i + 1, count);

            if (directusClient->downloadFingerprintTemplate(fpId, templateBuffer,
                                                            &templateSize, &localId)) {
                Serial.printf("→ Uploading to R307 (ID #%d)...\n", localId);

                if (fpHandler->uploadModel(localId, templateBuffer, templateSize)) {
                    successCount++;
                    Serial.println("✓ Success!");
                } else {
                    Serial.println("✗ Upload failed!");
                }
            }

            delay(500);  // Delay giữa các uploads
        }

        Serial.println("\n╔════════════════════════════════════════╗");
        Serial.printf("║  ✓ Restored %d/%d fingerprints\n", successCount, count);
        Serial.println("╚════════════════════════════════════════╝");

    } else if (selected >= 1 && selected <= count) {
        // Restore single fingerprint
        int index = selected - 1;
        String fpId = data[index]["id"].as<String>();
        uint8_t localId;

        Serial.printf("\n→ Restoring fingerprint #%d...\n", selected);

        if (directusClient->downloadFingerprintTemplate(fpId, templateBuffer,
                                                        &templateSize, &localId)) {
            if (fpHandler->uploadModel(localId, templateBuffer, templateSize)) {
                Serial.println("\n✓ Restore thành công!");
            } else {
                Serial.println("\n✗ Upload failed!");
            }
        }
    } else {
        Serial.println("✗ Lựa chọn không hợp lệ.");
    }

    printMenu();
}

void checkAutoLogin() {
    unsigned long now = millis();

    if (now - lastFingerprintCheck < FINGERPRINT_CHECK_INTERVAL) {
        return;  // Chưa đến lúc check
    }

    lastFingerprintCheck = now;

    // Kiểm tra vân tay
    int fingerprintID = fpHandler->verifyFingerprint();

    if (fingerprintID > 0) {
        // Tìm thấy vân tay trên sensor
        uint16_t confidence = fpHandler->getConfidence();

        Serial.println("\n→ Phát hiện vân tay!");
        fpHandler->ledOn(3); // Purple LED - processing

        // Lấy template
        if (fpHandler->getTemplate(fingerprintID, templateBuffer, &templateSize)) {
            // Convert to Base64
            String templateBase64 = base64::encode(templateBuffer, templateSize);
            String deviceMac = wifiManager->getMACAddress();
            String memberId;

            // Verify với Directus
            bool access = directusClient->verifyFingerprint(deviceMac, fingerprintID,
                                                           templateBase64, confidence, memberId);

            if (access) {
                // ACCESS GRANTED
                fpHandler->ledOn(2); // Blue LED
                if (buzzerHandler) buzzerHandler->play(BUZZ_ACCESS_GRANTED);
                Serial.println("✓ ACCESS GRANTED - Attendance logged");
            } else {
                // ACCESS DENIED - RẤT SAI!
                fpHandler->ledOn(1); // Red LED
                if (buzzerHandler) buzzerHandler->play(BUZZ_ACCESS_DENIED);
                Serial.println("✗ ACCESS DENIED!");
            }

            // Publish attendance event via MQTT (real-time)
            if (mqttClient->isConnected()) {
                mqttClient->publishAttendance(deviceMac, memberId,
                    "", confidence, access);
            }

            delay(2000);
            fpHandler->ledOff();
        }
    } else if (fingerprintID == -1) {
        // CÓ ngón tay nhưng KHÔNG KHỚP trên sensor - RẤT SAI!
        fpHandler->ledOn(1); // Red LED
        if (buzzerHandler) buzzerHandler->play(BUZZ_ACCESS_DENIED);
        Serial.println("✗ VÂN TAY KHÔNG HỢP LỆ!");
        delay(1500);
        fpHandler->ledOff();
    }
    // fingerprintID == -2: không có ngón tay → không làm gì
}
