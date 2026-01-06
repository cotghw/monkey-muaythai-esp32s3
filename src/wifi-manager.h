#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include "config.h"

/**
 * WiFiManager - Quản lý kết nối WiFi
 *
 * Chức năng:
 * - Kết nối WiFi với SSID/Password
 * - Kiểm tra trạng thái kết nối
 * - Reconnect tự động khi mất kết nối
 */
class WiFiManager {
public:
    WiFiManager();

    /**
     * Kết nối WiFi với credentials từ config.h
     * @return true nếu kết nối thành công, false nếu thất bại
     */
    bool connect();

    /**
     * Kết nối WiFi với custom SSID và password
     * @param ssid WiFi SSID
     * @param password WiFi password
     * @return true nếu kết nối thành công, false nếu thất bại
     */
    bool connect(const char* ssid, const char* password);

    /**
     * Ngắt kết nối WiFi
     */
    void disconnect();

    /**
     * Kiểm tra trạng thái kết nối WiFi
     * @return true nếu đang kết nối, false nếu mất kết nối
     */
    bool isConnected();

    /**
     * Lấy địa chỉ IP hiện tại
     * @return String chứa địa chỉ IP
     */
    String getIPAddress();

    /**
     * Lấy MAC address của thiết bị
     * @return String chứa MAC address
     */
    String getMACAddress();

    /**
     * In thông tin WiFi ra Serial
     */
    void printInfo();

private:
    String _ssid;
    String _password;
    unsigned long _connectTimeout;
};

#endif
