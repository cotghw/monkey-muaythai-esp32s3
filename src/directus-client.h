#ifndef DIRECTUS_CLIENT_H
#define DIRECTUS_CLIENT_H

#include "http-client.h"
#include "wifi-manager.h"
#include "offline-queue.h"
#include <ArduinoJson.h>

/**
 * DirectusClient - Directus API client cho fingerprint system
 *
 * Chức năng:
 * - Verify fingerprint với Directus
 * - Enroll fingerprint vào Directus
 * - Get/Update device info
 * - Create attendance logs
 */
class DirectusClient {
public:
    DirectusClient(HTTPClientManager* httpClient, WiFiManager* wifiManager,
                   OfflineQueue* offlineQueue = nullptr);

    /**
     * Verify fingerprint với Directus
     * @param deviceMac MAC address của ESP32
     * @param fingerprintID ID vân tay (1-127)
     * @param templateData Base64 encoded template
     * @param confidence Confidence score
     * @param memberId Output: Member UUID nếu verify thành công
     * @return true nếu access granted
     */
    bool verifyFingerprint(const String& deviceMac, uint8_t fingerprintID,
                          const String& templateData, uint16_t confidence,
                          String& memberId);

    /**
     * Enroll fingerprint mới vào Directus
     * @param deviceMac MAC address
     * @param fingerprintID ID vân tay
     * @param templateData Base64 template
     * @param memberId Member UUID
     * @return true nếu enroll thành công
     */
    bool enrollFingerprint(const String& deviceMac, uint8_t fingerprintID,
                          const String& templateData, const String& memberId);

    /**
     * Tạo hoặc cập nhật device info
     * @param deviceMac MAC address
     * @param deviceName Tên thiết bị
     * @param ipAddress IP address
     * @return Device UUID
     */
    String registerDevice(const String& deviceMac, const String& deviceName,
                         const String& ipAddress);

    /**
     * Log attendance (check in/out)
     * @param memberId Member UUID
     * @param deviceId Device UUID
     * @param fingerprintID ID vân tay
     * @param confidence Confidence score
     * @param accessGranted Access result
     * @param reason Lý do (success, low_confidence, not_registered...)
     * @return true nếu log thành công
     */
    bool logAttendance(const String& memberId, const String& deviceId,
                      uint8_t fingerprintID, uint16_t confidence,
                      bool accessGranted, const String& reason);

    /**
     * Get device UUID from MAC address
     * @param deviceMac MAC address
     * @return Device UUID hoặc empty string nếu không tìm thấy
     */
    String getDeviceId(const String& deviceMac);

    /**
     * Get list of fingerprints từ Directus cho device này
     * @param deviceMac MAC address
     * @param doc Output JsonDocument chứa array fingerprints
     * @return Số lượng fingerprints tìm thấy
     */
    int getFingerprints(const String& deviceMac, JsonDocument& doc);

    /**
     * Download một fingerprint template từ Directus
     * @param fingerprintId Fingerprint UUID
     * @param templateBuffer Output buffer (512 bytes)
     * @param templateSize Output size
     * @param fingerprintIdLocal Output local ID (1-127)
     * @return true nếu download thành công
     */
    bool downloadFingerprintTemplate(const String& fingerprintId,
                                     uint8_t* templateBuffer,
                                     uint16_t* templateSize,
                                     uint8_t* fingerprintIdLocal);

private:
    HTTPClientManager* _httpClient;
    WiFiManager* _wifiManager;
    OfflineQueue* _offlineQueue;
    String _deviceUuid;  // Cache device UUID

    /**
     * Build Directus API URL
     * @param endpoint API endpoint (VD: "/items/fingerprints")
     * @return Full URL
     */
    String buildUrl(const String& endpoint);

    /**
     * Tìm fingerprint match trong Directus database
     * @param deviceMac MAC address
     * @param templateData Template để so sánh
     * @param fingerprintId Output: Fingerprint UUID nếu tìm thấy
     * @param memberId Output: Member UUID nếu tìm thấy
     * @return true nếu tìm thấy match
     */
    bool findMatchingFingerprint(const String& deviceMac, const String& templateData,
                                String& fingerprintId, String& memberId);

    /**
     * So sánh 2 Base64 template strings
     * Simple comparison - trong production nên dùng Hamming distance
     * @param template1 Base64 string 1
     * @param template2 Base64 string 2
     * @return true nếu khớp (hoặc gần khớp)
     */
    bool compareTemplates(const String& template1, const String& template2);
};

#endif
