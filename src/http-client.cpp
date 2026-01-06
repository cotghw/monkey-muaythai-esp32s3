#include "http-client.h"

HTTPClientManager::HTTPClientManager() {
    _timeout = HTTP_TIMEOUT_MS;
}

int HTTPClientManager::post(const char* url, const String& jsonPayload, String& response) {
    _http.begin(url);
    _http.setTimeout(_timeout);
    _http.addHeader("Content-Type", "application/json");

    // Thêm Directus token
    if (strlen(DIRECTUS_TOKEN) > 0) {
        _http.addHeader("Authorization", String("Bearer ") + DIRECTUS_TOKEN);
    }

    Serial.println("\n→ Đang gửi POST request...");
    Serial.print("URL: ");
    Serial.println(url);
    Serial.print("Payload: ");
    Serial.println(jsonPayload);

    int httpCode = _http.POST(jsonPayload);

    if (httpCode > 0) {
        response = _http.getString();
        Serial.print("HTTP Code: ");
        Serial.println(httpCode);
        Serial.print("Response: ");
        Serial.println(response);
    } else {
        Serial.print("✗ HTTP Error: ");
        Serial.println(_http.errorToString(httpCode));
    }

    _http.end();
    return httpCode;
}

int HTTPClientManager::get(const char* url, String& response) {
    _http.begin(url);
    _http.setTimeout(_timeout);

    // Thêm Directus token
    if (strlen(DIRECTUS_TOKEN) > 0) {
        _http.addHeader("Authorization", String("Bearer ") + DIRECTUS_TOKEN);
    }

    Serial.println("\n→ Đang gửi GET request...");
    Serial.print("URL: ");
    Serial.println(url);

    int httpCode = _http.GET();

    if (httpCode > 0) {
        response = _http.getString();
        Serial.print("HTTP Code: ");
        Serial.println(httpCode);
        Serial.print("Response: ");
        Serial.println(response);
    } else {
        Serial.print("✗ HTTP Error: ");
        Serial.println(_http.errorToString(httpCode));
    }

    _http.end();
    return httpCode;
}

bool HTTPClientManager::parseJSON(const String& response, JsonDocument& doc) {
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        Serial.print("✗ JSON Parse Error: ");
        Serial.println(error.c_str());
        return false;
    }

    return true;
}

String HTTPClientManager::createJSON(const JsonDocument& doc) {
    String output;
    serializeJson(doc, output);
    return output;
}
