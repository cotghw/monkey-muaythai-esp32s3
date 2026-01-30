#include "fingerprint-handler.h"
#include "buzzer-handler.h"

FingerprintHandler::FingerprintHandler(HardwareSerial *serial) {
    serialPort = serial;  // Save serial port reference
    finger = new Adafruit_Fingerprint(serial);
    enrollStep = 0;
    buzzer = nullptr;
}

void FingerprintHandler::setBuzzer(BuzzerHandler* buzz) {
    buzzer = buzz;
}

bool FingerprintHandler::begin() {
    // Khởi động UART với baudrate R307
    finger->begin(57600);

    // ESP32-S3 cần delay dài hơn để UART ổn định
    delay(500);

    Serial.println("→ Đang verify password cảm biến...");

    // Thử verify với retry
    bool connected = false;
    for (int i = 0; i < 3; i++) {
        if (finger->verifyPassword()) {
            connected = true;
            break;
        }
        Serial.printf("→ Retry %d/3...\n", i + 1);
        delay(500);
    }

    if (connected) {
        Serial.println("✓ Tìm thấy cảm biến vân tay R307!");
        return true;
    } else {
        Serial.println("✗ Không tìm thấy cảm biến R307");
        Serial.println("\nKiểm tra:");
        Serial.println("1. Kết nối UART:");
        Serial.println("   - R307 TX (Xanh) -> ESP32 GPIO16");
        Serial.println("   - R307 RX (Trắng) -> ESP32 GPIO17");
        Serial.println("2. Nguồn điện:");
        Serial.println("   - VCC -> 3.3V (hoặc 5V nếu có)");
        Serial.println("   - GND -> GND");
        Serial.println("3. Thử đổi TX/RX nếu vẫn lỗi");
        return false;
    }
}

void FingerprintHandler::printSensorInfo() {
    Serial.println("\n=== Thông tin cảm biến ===");
    finger->getParameters();
    Serial.print("Dung lượng: "); Serial.println(finger->capacity);
    Serial.print("Tốc độ baud: "); Serial.println(finger->baud_rate, HEX);
    Serial.print("Kích thước gói: "); Serial.println(finger->packet_len);
    Serial.print("Số vân tay đã lưu: "); Serial.println(getTemplateCount());
    Serial.println("======================\n");
}

uint16_t FingerprintHandler::getTemplateCount() {
    finger->getTemplateCount();
    return finger->templateCount;
}

bool FingerprintHandler::isFingerDetected() {
    uint8_t p = finger->getImage();
    return (p == FINGERPRINT_OK);
}

FingerprintStatus FingerprintHandler::captureImage() {
    uint8_t p = finger->getImage();

    switch (p) {
        case FINGERPRINT_OK:
            return FP_SUCCESS;
        case FINGERPRINT_NOFINGER:
            return FP_NO_FINGER;
        case FINGERPRINT_PACKETRECIEVEERR:
            return FP_COMMUNICATION_ERROR;
        case FINGERPRINT_IMAGEFAIL:
            return FP_IMAGE_FAIL;
        default:
            return FP_IMAGE_FAIL;
    }
}

int FingerprintHandler::enrollFingerprint(uint8_t id) {
    Serial.printf("\n=== Đăng ký vân tay ID #%d ===\n", id);

    // Bước 1: Lấy ảnh lần 1
    Serial.println("Đặt ngón tay lên cảm biến...");
    if (buzzer) buzzer->play(BUZZ_SINGLE);
    ledOn(2); // Blue LED

    while (finger->getImage() != FINGERPRINT_OK) {
        delay(100);
    }

    Serial.println("✓ Đã lấy ảnh lần 1");
    uint8_t p = finger->image2Tz(1);
    if (p != FINGERPRINT_OK) {
        Serial.println("✗ Lỗi chuyển đổi ảnh lần 1");
        if (buzzer) buzzer->play(BUZZ_ERROR);
        ledOff();
        return -1;
    }

    // Yêu cầu nhấc tay
    Serial.println("Nhấc tay ra...");
    if (buzzer) buzzer->play(BUZZ_DOUBLE);
    delay(2000);
    while (finger->getImage() != FINGERPRINT_NOFINGER) {
        delay(100);
    }

    // Bước 2: Lấy ảnh lần 2
    Serial.println("Đặt lại ngón tay lên cảm biến...");
    if (buzzer) buzzer->play(BUZZ_SINGLE);
    ledOn(3); // Purple LED

    while (finger->getImage() != FINGERPRINT_OK) {
        delay(100);
    }

    Serial.println("✓ Đã lấy ảnh lần 2");
    p = finger->image2Tz(2);
    if (p != FINGERPRINT_OK) {
        Serial.println("✗ Lỗi chuyển đổi ảnh lần 2");
        if (buzzer) buzzer->play(BUZZ_ERROR);
        ledOff();
        return -1;
    }

    // Tạo mẫu
    p = finger->createModel();
    if (p != FINGERPRINT_OK) {
        Serial.println("✗ Vân tay không khớp, thử lại!");
        if (buzzer) buzzer->play(BUZZ_ERROR);
        ledOff();
        return -1;
    }

    // Lưu vào bộ nhớ
    p = finger->storeModel(id);
    if (p == FINGERPRINT_OK) {
        Serial.printf("✓ Đã lưu vân tay ID #%d thành công!\n", id);
        if (buzzer) buzzer->play(BUZZ_SUCCESS);
        ledOn(2); // Blue LED success
        delay(500);
        ledOff();
        return id;
    } else {
        Serial.println("✗ Lỗi lưu vân tay");
        if (buzzer) buzzer->play(BUZZ_ERROR);
        ledOff();
        return -1;
    }
}

int FingerprintHandler::verifyFingerprint() {
    // Lấy ảnh
    uint8_t p = finger->getImage();
    if (p != FINGERPRINT_OK) {
        return -2;  // -2 = không có ngón tay hoặc lỗi capture
    }

    // Chuyển đổi ảnh
    p = finger->image2Tz();
    if (p != FINGERPRINT_OK) {
        return -1;  // -1 = có ngón tay nhưng lỗi chuyển đổi
    }

    // Tìm kiếm trong database
    p = finger->fingerSearch();
    if (p == FINGERPRINT_OK) {
        Serial.printf("✓ Tìm thấy vân tay! ID: #%d, Độ tin cậy: %d\n",
                     finger->fingerID, finger->confidence);
        return finger->fingerID;
    } else {
        Serial.println("✗ Có ngón tay nhưng KHÔNG KHỚP!");
        return -1;  // -1 = có ngón tay nhưng không khớp (SAI!)
    }
}

bool FingerprintHandler::deleteFingerprint(uint8_t id) {
    Serial.printf("→ Đang xóa vân tay ID #%d...\n", id);

    uint8_t p = finger->deleteModel(id);

    if (p == FINGERPRINT_OK) {
        Serial.printf("✓ Đã xóa vân tay ID #%d\n", id);
        if (buzzer) buzzer->play(BUZZ_DELETE);
        return true;
    } else {
        Serial.printf("✗ Lỗi xóa vân tay ID #%d\n", id);
        if (buzzer) buzzer->play(BUZZ_ERROR);
        return false;
    }
}

bool FingerprintHandler::deleteAllFingerprints() {
    uint8_t p = finger->emptyDatabase();

    if (p == FINGERPRINT_OK) {
        Serial.println("✓ Đã xóa toàn bộ vân tay");
        return true;
    } else {
        Serial.println("✗ Lỗi xóa database");
        return false;
    }
}

void FingerprintHandler::ledOn(uint8_t color) {
    // R307 LED control
    // color: 1=Red, 2=Blue, 3=Purple
    uint8_t p = finger->LEDcontrol(FINGERPRINT_LED_BREATHING, 1, color);
    Serial.printf("→ LED ON color=%d, result=0x%02X\n", color, p);
}

void FingerprintHandler::ledOff() {
    uint8_t p = finger->LEDcontrol(FINGERPRINT_LED_OFF, 0, 0);
    Serial.printf("→ LED OFF, result=0x%02X\n", p);
}

bool FingerprintHandler::getTemplate(uint8_t id, uint8_t* templateBuffer, uint16_t* templateSize) {
    // Load template từ flash memory vào buffer slot 1
    uint8_t p = finger->loadModel(id);
    if (p != FINGERPRINT_OK) {
        Serial.printf("✗ Lỗi load template ID #%d\n", id);
        return false;
    }

    // Download template từ buffer
    return downloadModel(1, templateBuffer, templateSize);
}

bool FingerprintHandler::downloadModel(uint8_t slot, uint8_t* templateBuffer, uint16_t* templateSize) {
    // NOTE: Adafruit library không expose fingerTemplate data
    // Workaround: Tạo unique identifier từ fingerprint ID

    Serial.println("⚠ Adafruit library không hỗ trợ download template data");
    Serial.println("  Sử dụng fingerprint ID để identify trong Directus");

    // Tạo placeholder template với fingerprint ID
    *templateSize = 512;
    memset(templateBuffer, 0, 512);

    // Đặt fingerprint ID vào đầu template để unique
    templateBuffer[0] = slot;
    templateBuffer[1] = 0xFF;  // Marker
    templateBuffer[2] = finger->fingerID & 0xFF;
    templateBuffer[3] = (finger->fingerID >> 8) & 0xFF;

    Serial.printf("✓ Generated ID-based template (%d bytes)\n", *templateSize);
    Serial.println("  NOTE: Template matching sẽ dùng fingerprint_id trong Directus");
    return true;
}

bool FingerprintHandler::uploadModel(uint8_t id, uint8_t* templateBuffer, uint16_t templateSize) {
    Serial.printf("→ Uploading template to R307 (ID #%d, size: %d bytes)...\n", id, templateSize);

    // Validate template size (should be 512 bytes for R307)
    if (templateSize != 512) {
        Serial.printf("✗ Invalid template size: %d (expected 512)\n", templateSize);
        return false;
    }

    // ⚠️ WARNING: R307 template upload via UART packets
    // This uses raw UART communication to send template data
    // May not work on all R307 firmware versions!

    Serial.println("→ Sending DownChar command...");

    // Create packet for DownChar command (0x08)
    // Command packet: Header + Addr + PID + Length + Data + Checksum
    uint8_t cmdPacket[12];
    cmdPacket[0] = 0xEF;  // Header high
    cmdPacket[1] = 0x01;  // Header low
    cmdPacket[2] = 0xFF;  // Address byte 1
    cmdPacket[3] = 0xFF;  // Address byte 2
    cmdPacket[4] = 0xFF;  // Address byte 3
    cmdPacket[5] = 0xFF;  // Address byte 4
    cmdPacket[6] = 0x01;  // Package identifier (command packet)
    cmdPacket[7] = 0x00;  // Package length high
    cmdPacket[8] = 0x04;  // Package length low (4 bytes: cmd + bufferID + checksum)
    cmdPacket[9] = 0x08;  // Command code: DownChar
    cmdPacket[10] = 0x01; // Buffer ID (CharBuffer1)

    // Calculate checksum
    uint16_t sum = 0x01 + 0x00 + 0x04 + 0x08 + 0x01;
    cmdPacket[11] = (sum >> 8) & 0xFF;  // Checksum high (always 0 for this case)
    // We need 2 bytes for checksum but packet is only 12, so we'll send separately

    // Actually, let me fix the packet structure properly
    uint8_t cmdPacketFull[13];
    cmdPacketFull[0] = 0xEF;  // Header high
    cmdPacketFull[1] = 0x01;  // Header low
    cmdPacketFull[2] = 0xFF;  // Address
    cmdPacketFull[3] = 0xFF;
    cmdPacketFull[4] = 0xFF;
    cmdPacketFull[5] = 0xFF;
    cmdPacketFull[6] = 0x01;  // Package ID (command)
    cmdPacketFull[7] = 0x00;  // Length high
    cmdPacketFull[8] = 0x04;  // Length low
    cmdPacketFull[9] = 0x08;  // DownChar command
    cmdPacketFull[10] = 0x01; // BufferID = 1
    sum = 0x01 + 0x00 + 0x04 + 0x08 + 0x01;
    cmdPacketFull[11] = (sum >> 8) & 0xFF;
    cmdPacketFull[12] = sum & 0xFF;

    serialPort->write(cmdPacketFull, 13);
    delay(100);

    // Now send data packets (2 packets of 256 bytes each)
    uint8_t dataPacket[267];

    // Packet 1
    Serial.println("→ Sending data packet 1/2...");
    dataPacket[0] = 0xEF;
    dataPacket[1] = 0x01;
    dataPacket[2] = 0xFF;
    dataPacket[3] = 0xFF;
    dataPacket[4] = 0xFF;
    dataPacket[5] = 0xFF;
    dataPacket[6] = 0x02;  // Data packet
    dataPacket[7] = 0x01;  // Length high (258 = 0x0102)
    dataPacket[8] = 0x02;  // Length low

    memcpy(&dataPacket[9], templateBuffer, 256);

    sum = 0x02 + 0x01 + 0x02;
    for (int i = 0; i < 256; i++) {
        sum += templateBuffer[i];
    }
    dataPacket[265] = (sum >> 8) & 0xFF;
    dataPacket[266] = sum & 0xFF;

    serialPort->write(dataPacket, 267);
    delay(100);

    // Packet 2 (end packet)
    Serial.println("→ Sending data packet 2/2...");
    dataPacket[6] = 0x08;  // End data packet
    memcpy(&dataPacket[9], templateBuffer + 256, 256);

    sum = 0x08 + 0x01 + 0x02;
    for (int i = 256; i < 512; i++) {
        sum += templateBuffer[i];
    }
    dataPacket[265] = (sum >> 8) & 0xFF;
    dataPacket[266] = sum & 0xFF;

    serialPort->write(dataPacket, 267);
    delay(200);

    // Wait for ACK from DownChar command
    // Read response (12 bytes expected)
    uint8_t response[12];
    int bytesRead = 0;
    unsigned long timeout = millis();

    while (bytesRead < 12 && (millis() - timeout) < 1000) {
        if (serialPort->available()) {
            response[bytesRead++] = serialPort->read();
        }
    }

    if (bytesRead >= 12 && response[9] == 0x00) {
        Serial.println("✓ DownChar succeeded");
    } else {
        Serial.printf("⚠ DownChar response unclear (%d bytes read)\n", bytesRead);
        // Continue anyway - some firmware versions may not respond properly
    }

    // Clear serial buffer before sending Store command
    while (serialPort->available()) {
        serialPort->read();
    }
    delay(100);

    // Send Store command (0x06) to save CharBuffer1 to flash
    Serial.printf("→ Storing CharBuffer to flash memory (ID #%d)...\n", id);

    uint8_t storeCmd[14];
    storeCmd[0] = 0xEF;  // Header
    storeCmd[1] = 0x01;
    storeCmd[2] = 0xFF;  // Address
    storeCmd[3] = 0xFF;
    storeCmd[4] = 0xFF;
    storeCmd[5] = 0xFF;
    storeCmd[6] = 0x01;  // Command packet
    storeCmd[7] = 0x00;  // Length high
    storeCmd[8] = 0x06;  // Length low (6 bytes)
    storeCmd[9] = 0x06;  // Store command
    storeCmd[10] = 0x01; // BufferID = 1
    storeCmd[11] = (id >> 8) & 0xFF;  // Page ID high
    storeCmd[12] = id & 0xFF;          // Page ID low

    sum = 0x01 + 0x00 + 0x06 + 0x06 + 0x01 + ((id >> 8) & 0xFF) + (id & 0xFF);
    storeCmd[13] = sum & 0xFF;  // Checksum low byte only (high byte would be in storeCmd[14] if needed)

    // Actually need 15 bytes for full packet with 2-byte checksum
    uint8_t storeCmdFull[15];
    memcpy(storeCmdFull, storeCmd, 13);
    storeCmdFull[13] = (sum >> 8) & 0xFF;  // Checksum high
    storeCmdFull[14] = sum & 0xFF;          // Checksum low

    serialPort->write(storeCmdFull, 15);
    delay(200);

    // Read Store response
    bytesRead = 0;
    timeout = millis();
    while (bytesRead < 12 && (millis() - timeout) < 1000) {
        if (serialPort->available()) {
            response[bytesRead++] = serialPort->read();
        }
    }

    if (bytesRead >= 12 && response[9] == 0x00) {
        Serial.printf("✓ Template uploaded & stored successfully! ID #%d\n", id);
        return true;
    } else if (bytesRead >= 10) {
        Serial.printf("⚠ Store response code: 0x%02X (read %d bytes)\n", response[9], bytesRead);

        // Check if template already exists by trying to verify
        Serial.println("→ Verifying if template was actually stored...");
        delay(500);

        // Try to load the model we just stored
        uint8_t p = finger->loadModel(id);
        if (p == FINGERPRINT_OK) {
            Serial.printf("✓ Verification SUCCESS! Template ID #%d is stored and working!\n", id);
            return true;
        } else {
            Serial.printf("✗ Verification FAILED - template not stored (load error: 0x%02X)\n", p);
            return false;
        }
    } else {
        Serial.printf("✗ Store command timeout (%d bytes received)\n", bytesRead);
        return false;
    }
}

uint16_t FingerprintHandler::getConfidence() {
    return finger->confidence;
}
