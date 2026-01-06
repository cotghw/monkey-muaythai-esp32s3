#include "directus-client.h"
#include "config.h"
#include <base64.h>
#include <mbedtls/base64.h>
#include <time.h>

DirectusClient::DirectusClient(HTTPClientManager* httpClient, WiFiManager* wifiManager) {
    _httpClient = httpClient;
    _wifiManager = wifiManager;
    _deviceUuid = "";  // Will be loaded on first request
}

String DirectusClient::buildUrl(const String& endpoint) {
    return String(DIRECTUS_URL) + endpoint;
}

String DirectusClient::getDeviceId(const String& deviceMac) {
    // Return cached UUID if available
    if (_deviceUuid.length() > 0) {
        return _deviceUuid;
    }

    // Query Directus for device by MAC address
    String url = buildUrl("/items/devices?filter[device_mac][_eq]=" + deviceMac);
    String response;

    int httpCode = _httpClient->get(url.c_str(), response);

    if (httpCode == 200) {
        JsonDocument doc;
        if (_httpClient->parseJSON(response, doc)) {
            JsonArray data = doc["data"];
            if (data.size() > 0) {
                _deviceUuid = data[0]["id"].as<String>();
                Serial.print("✓ Device UUID: ");
                Serial.println(_deviceUuid);
                return _deviceUuid;
            }
        }
    }

    Serial.println("⚠ Device chưa được đăng ký trong Directus");
    return "";
}

String DirectusClient::registerDevice(const String& deviceMac, const String& deviceName,
                                     const String& ipAddress) {
    // Check if device exists
    String existingId = getDeviceId(deviceMac);
    if (existingId.length() > 0) {
        // Update existing device
        JsonDocument doc;
        doc["last_seen_at"] = "now";
        doc["ip_address"] = ipAddress;
        doc["status"] = "online";

        String jsonPayload = _httpClient->createJSON(doc);
        String url = buildUrl("/items/devices/" + existingId);
        String response;

        // Note: Directus PATCH requires different HTTP method
        // For now, skip update or implement PATCH support
        Serial.println("✓ Device đã tồn tại, skip update");
        return existingId;
    }

    // Create new device
    JsonDocument doc;
    doc["device_mac"] = deviceMac;
    doc["device_name"] = deviceName;
    doc["ip_address"] = ipAddress;
    doc["status"] = "online";
    doc["is_active"] = true;

    String jsonPayload = _httpClient->createJSON(doc);
    String url = buildUrl("/items/devices");
    String response;

    int httpCode = _httpClient->post(url.c_str(), jsonPayload, response);

    if (httpCode == 200 || httpCode == 201) {
        JsonDocument responseDoc;
        if (_httpClient->parseJSON(response, responseDoc)) {
            _deviceUuid = responseDoc["data"]["id"].as<String>();
            Serial.println("✓ Đăng ký device thành công!");
            return _deviceUuid;
        }
    }

    Serial.println("✗ Lỗi đăng ký device");
    return "";
}

bool DirectusClient::compareTemplates(const String& template1, const String& template2) {
    // Simple exact match
    // TODO: Implement Hamming distance algorithm for better matching
    return template1 == template2;
}

bool DirectusClient::findMatchingFingerprint(const String& deviceMac, const String& templateData,
                                            String& fingerprintId, String& employeeId) {
    // NOTE: Simplified version - Skip template matching
    // R307 sensor đã verify fingerprint, chúng ta chỉ cần log vào Directus

    Serial.println("✓ Fingerprint đã được verify bởi R307 sensor");
    Serial.println("  Skip Directus template matching (Adafruit library limitation)");

    // Return placeholder values
    // Actual employee tracking sẽ được handle sau khi enroll vào Directus
    fingerprintId = "verified";
    employeeId = "unknown";

    return true;
}

bool DirectusClient::verifyFingerprint(const String& deviceMac, uint8_t fingerprintID,
                                      const String& templateData, uint16_t confidence,
                                      String& employeeId) {
    if (!_wifiManager->isConnected()) {
        Serial.println("✗ WiFi chưa kết nối!");
        return false;
    }

    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║   ĐANG XÁC THỰC VỚI DIRECTUS...       ║");
    Serial.println("╚════════════════════════════════════════╝");

    // Tìm fingerprint match
    String fingerprintId;
    if (!findMatchingFingerprint(deviceMac, templateData, fingerprintId, employeeId)) {
        // Log failed attempt
        String deviceId = getDeviceId(deviceMac);
        logAttendance("", deviceId, fingerprintID, confidence, false, "not_registered");
        return false;
    }

    // Check confidence threshold
    // TODO: Get threshold from fingerprint record
    const uint16_t MIN_CONFIDENCE = 80;
    if (confidence < MIN_CONFIDENCE) {
        Serial.printf("✗ Confidence quá thấp (%d < %d)\n", confidence, MIN_CONFIDENCE);

        String deviceId = getDeviceId(deviceMac);
        logAttendance(employeeId, deviceId, fingerprintID, confidence, false, "low_confidence");
        return false;
    }

    // Access granted!
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║       ✓ ACCESS GRANTED!               ║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.printf("║   Employee ID: %s\n", employeeId.c_str());
    Serial.printf("║   Confidence: %d                       ║\n", confidence);
    Serial.println("╚════════════════════════════════════════╝");

    // Log successful attendance
    String deviceId = getDeviceId(deviceMac);
    logAttendance(employeeId, deviceId, fingerprintID, confidence, true, "success");

    return true;
}

bool DirectusClient::enrollFingerprint(const String& deviceMac, uint8_t fingerprintID,
                                      const String& templateData, const String& employeeCode) {
    if (!_wifiManager->isConnected()) {
        Serial.println("✗ WiFi chưa kết nối!");
        return false;
    }

    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║  ĐĂNG KÝ VÂN TAY LÊN DIRECTUS...      ║");
    Serial.println("╚════════════════════════════════════════╝");

    // Get device UUID
    String deviceId = getDeviceId(deviceMac);
    if (deviceId.length() == 0) {
        Serial.println("✗ Device chưa được đăng ký!");
        return false;
    }

    // Get employee UUID from employee_code
    String url = buildUrl("/items/employees?filter[employee_code][_eq]=" + employeeCode);
    String response;

    int httpCode = _httpClient->get(url.c_str(), response);

    String employeeId = "";
    if (httpCode == 200) {
        JsonDocument doc;
        if (_httpClient->parseJSON(response, doc)) {
            JsonArray data = doc["data"];
            if (data.size() > 0) {
                employeeId = data[0]["id"].as<String>();
            }
        }
    }

    if (employeeId.length() == 0) {
        Serial.printf("✗ Không tìm thấy employee code: %s\n", employeeCode.c_str());
        return false;
    }

    // Create fingerprint record
    JsonDocument enrollDoc;
    enrollDoc["employee_id"] = employeeId;
    enrollDoc["device_id"] = deviceId;
    enrollDoc["fingerprint_id"] = fingerprintID;
    enrollDoc["template_data"] = templateData;
    enrollDoc["is_active"] = true;

    // Get current timestamp from WiFiManager (NTP sync)
    // Format: ISO 8601 (YYYY-MM-DDTHH:MM:SS.sssZ)
    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    char timestamp[30];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S.000Z", &timeinfo);
    enrollDoc["enrolled_at"] = timestamp;

    String jsonPayload = _httpClient->createJSON(enrollDoc);
    url = buildUrl("/items/fingerprints");

    httpCode = _httpClient->post(url.c_str(), jsonPayload, response);

    if (httpCode == 200 || httpCode == 201) {
        Serial.println("\n✓ Đăng ký vân tay lên Directus thành công!");
        return true;
    }

    Serial.printf("✗ Lỗi đăng ký (HTTP %d)\n", httpCode);
    return false;
}

bool DirectusClient::logAttendance(const String& employeeId, const String& deviceId,
                                  uint8_t fingerprintID, uint16_t confidence,
                                  bool accessGranted, const String& reason) {
    JsonDocument doc;

    if (employeeId.length() > 0 && employeeId != "unknown") {
        doc["employee_id"] = employeeId;
    }

    doc["device_id"] = deviceId;
    doc["fingerprint_id"] = fingerprintID;
    doc["confidence"] = confidence;
    doc["access_granted"] = accessGranted;
    doc["reason"] = reason;

    // Add current timestamp
    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    char timestamp[30];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S.000Z", &timeinfo);
    doc["timestamp"] = timestamp;

    String jsonPayload = _httpClient->createJSON(doc);
    String url = buildUrl("/items/attendance_logs");
    String response;

    int httpCode = _httpClient->post(url.c_str(), jsonPayload, response);

    if (httpCode == 200 || httpCode == 201) {
        Serial.println("✓ Đã log attendance");
        return true;
    }

    Serial.printf("⚠ Lỗi log attendance (HTTP %d)\n", httpCode);
    return false;
}

int DirectusClient::getFingerprints(const String& deviceMac, JsonDocument& doc) {
    if (!_wifiManager->isConnected()) {
        Serial.println("✗ WiFi chưa kết nối!");
        return 0;
    }

    // Get device UUID
    String deviceId = getDeviceId(deviceMac);
    if (deviceId.length() == 0) {
        Serial.println("✗ Device chưa được đăng ký!");
        return 0;
    }

    // Query fingerprints for this device
    String url = buildUrl("/items/fingerprints?filter[device_id][_eq]=" + deviceId +
                         "&filter[is_active][_eq]=true&limit=127");
    String response;

    int httpCode = _httpClient->get(url.c_str(), response);

    if (httpCode == 200) {
        if (_httpClient->parseJSON(response, doc)) {
            JsonArray data = doc["data"];
            Serial.printf("✓ Tìm thấy %d fingerprints trong Directus\n", data.size());
            return data.size();
        }
    }

    Serial.printf("✗ Lỗi query fingerprints (HTTP %d)\n", httpCode);
    return 0;
}

bool DirectusClient::downloadFingerprintTemplate(const String& fingerprintId,
                                                 uint8_t* templateBuffer,
                                                 uint16_t* templateSize,
                                                 uint8_t* fingerprintIdLocal) {
    if (!_wifiManager->isConnected()) {
        Serial.println("✗ WiFi chưa kết nối!");
        return false;
    }

    // Get fingerprint record
    String url = buildUrl("/items/fingerprints/" + fingerprintId);
    String response;

    int httpCode = _httpClient->get(url.c_str(), response);

    if (httpCode == 200) {
        JsonDocument doc;
        if (_httpClient->parseJSON(response, doc)) {
            String templateDataBase64 = doc["data"]["template_data"].as<String>();
            *fingerprintIdLocal = doc["data"]["fingerprint_id"].as<uint8_t>();

            if (templateDataBase64.length() == 0) {
                Serial.println("✗ Template data rỗng!");
                return false;
            }

            // Decode Base64 using mbedtls
            size_t outputLen = 0;
            unsigned char tempBuffer[600];  // Base64 expands ~33%, so 512 bytes -> ~683 chars

            int ret = mbedtls_base64_decode(tempBuffer, sizeof(tempBuffer), &outputLen,
                                           (const unsigned char*)templateDataBase64.c_str(),
                                           templateDataBase64.length());

            if (ret != 0) {
                Serial.printf("✗ Base64 decode failed (error: %d)\n", ret);
                return false;
            }

            *templateSize = outputLen;

            if (*templateSize > 512) {
                Serial.printf("✗ Template size quá lớn: %d bytes\n", *templateSize);
                return false;
            }

            // Copy to buffer
            memcpy(templateBuffer, tempBuffer, *templateSize);

            // Pad with zeros if needed
            if (*templateSize < 512) {
                memset(templateBuffer + *templateSize, 0, 512 - *templateSize);
                *templateSize = 512;
            }

            Serial.printf("✓ Downloaded template (ID local: %d, size: %d bytes)\n",
                         *fingerprintIdLocal, *templateSize);
            return true;
        }
    }

    Serial.printf("✗ Lỗi download fingerprint (HTTP %d)\n", httpCode);
    return false;
}
