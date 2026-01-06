#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

/**
 * HTTPClientManager - Quản lý HTTP requests đến CMS
 *
 * Chức năng:
 * - Gửi POST request với JSON payload
 * - Parse JSON response từ server
 * - Handle timeout và errors
 */
class HTTPClientManager {
public:
    HTTPClientManager();

    /**
     * Gửi POST request với JSON payload
     * @param url API endpoint URL
     * @param jsonPayload String chứa JSON payload
     * @param response String để lưu response từ server
     * @return HTTP status code (200 = OK)
     */
    int post(const char* url, const String& jsonPayload, String& response);

    /**
     * Gửi GET request
     * @param url API endpoint URL
     * @param response String để lưu response từ server
     * @return HTTP status code (200 = OK)
     */
    int get(const char* url, String& response);

    /**
     * Parse JSON response thành JsonDocument
     * @param response String chứa JSON response
     * @param doc JsonDocument để lưu kết quả parse
     * @return true nếu parse thành công
     */
    bool parseJSON(const String& response, JsonDocument& doc);

    /**
     * Tạo JSON payload từ JsonDocument
     * @param doc JsonDocument chứa data
     * @return String chứa JSON serialized
     */
    String createJSON(const JsonDocument& doc);

private:
    HTTPClient _http;
    unsigned long _timeout;
};

#endif
