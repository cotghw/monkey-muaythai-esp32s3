#include "wifi-manager.h"
#include <time.h>

WiFiManager::WiFiManager() {
    _connectTimeout = WIFI_TIMEOUT_MS;
}

bool WiFiManager::connect() {
    return connect(WIFI_SSID, WIFI_PASSWORD);
}

bool WiFiManager::connect(const char* ssid, const char* password) {
    _ssid = String(ssid);
    _password = String(password);

    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║       ĐANG KẾT NỐI WiFi...             ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.print("SSID: ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");

        if (millis() - startTime > _connectTimeout) {
            Serial.println("\n✗ TIMEOUT: Không thể kết nối WiFi!");
            Serial.println("Kiểm tra SSID/Password trong config.h");
            return false;
        }
    }

    Serial.println("\n\n╔════════════════════════════════════════╗");
    Serial.println("║       ✓ KẾT NỐI WiFi THÀNH CÔNG!      ║");
    Serial.println("╚════════════════════════════════════════╝");
    printInfo();

    // Setup NTP time sync
    Serial.println("\n→ Đang đồng bộ thời gian với NTP...");
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");  // GMT+7 Vietnam

    // Wait for time sync
    int retries = 0;
    while (time(nullptr) < 100000 && retries < 10) {
        delay(500);
        Serial.print(".");
        retries++;
    }

    if (time(nullptr) > 100000) {
        time_t now = time(nullptr);
        Serial.printf("\n✓ Thời gian đã đồng bộ: %s", ctime(&now));
    } else {
        Serial.println("\n⚠ Không thể đồng bộ thời gian NTP");
    }

    return true;
}

void WiFiManager::disconnect() {
    WiFi.disconnect();
    Serial.println("WiFi đã ngắt kết nối");
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getIPAddress() {
    return WiFi.localIP().toString();
}

String WiFiManager::getMACAddress() {
    return WiFi.macAddress();
}

void WiFiManager::printInfo() {
    Serial.print("IP Address: ");
    Serial.println(getIPAddress());
    Serial.print("MAC Address: ");
    Serial.println(getMACAddress());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
}
