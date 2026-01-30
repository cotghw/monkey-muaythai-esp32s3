#include "mqtt-client.h"
#include "config.h"
#include <time.h>

// Static instance for callback
MQTTClient* MQTTClient::instance = nullptr;

MQTTClient::MQTTClient(WiFiManager* wifi) :
    _wifi(wifi),
    _client(_wifiClient),
    _lastReconnect(0),
    _reconnectRetries(0),
    _isConnected(false),
    _port(1883)
{
    instance = this;
    _client.setCallback(MQTTClient::messageCallback);
}

bool MQTTClient::begin(const char* broker, uint16_t port, const char* username,
                       const char* password, const String& deviceId) {
    _broker = String(broker);
    _port = port;
    _username = String(username);
    _password = String(password);
    _deviceId = deviceId;

    setupTopics();

    _client.setServer(_broker.c_str(), _port);
    _client.setBufferSize(1024);  // Increase buffer for large JSON payloads
    _client.setKeepAlive(60);     // 60 seconds keepalive

    Serial.println("[MQTT] Configuration:");
    Serial.print("  Broker: ");
    Serial.print(_broker);
    Serial.print(":");
    Serial.println(_port);
    Serial.print("  Device ID: ");
    Serial.println(_deviceId);
    Serial.print("  Command Topic: ");
    Serial.println(_commandTopic);
    Serial.print("  Status Topic: ");
    Serial.println(_statusTopic);

    return true;
}

void MQTTClient::setupTopics() {
    // Use MAC as-is (with colons, uppercase) to match Directus device_mac field
    // Directus Flow builds topic: device/{{ device_mac }}/commands
    char buffer[128];

    snprintf(buffer, sizeof(buffer), MQTT_TOPIC_COMMANDS_TEMPLATE, _deviceId.c_str());
    _commandTopic = String(buffer);

    snprintf(buffer, sizeof(buffer), MQTT_TOPIC_STATUS_TEMPLATE, _deviceId.c_str());
    _statusTopic = String(buffer);

    snprintf(buffer, sizeof(buffer), MQTT_TOPIC_TELEMETRY_TEMPLATE, _deviceId.c_str());
    _telemetryTopic = String(buffer);

    _attendanceTopic = MQTT_TOPIC_ATTENDANCE_LIVE;
    _lwtTopic = _statusTopic;
}

void MQTTClient::loop() {
    if (!_wifi->isConnected()) {
        _isConnected = false;
        return;
    }

    if (!_client.connected()) {
        _isConnected = false;
        reconnect();
    }

    if (_client.connected()) {
        _client.loop();
        _isConnected = true;
    }
}

void MQTTClient::reconnect() {
    unsigned long now = millis();

    if (now - _lastReconnect < getReconnectDelay()) {
        return;
    }

    _lastReconnect = now;

    Serial.print("[MQTT] Connecting to broker... ");

    // Create Last Will & Testament (LWT) message
    JsonDocument lwtDoc;
    lwtDoc["status"] = "offline";
    lwtDoc["timestamp"] = now / 1000;
    String lwtPayload;
    serializeJson(lwtDoc, lwtPayload);

    // Generate client ID with device ID
    String clientId = "ESP32-" + _deviceId;

    bool connected = false;

    if (_username.length() > 0 && _password.length() > 0) {
        // Connect with authentication and LWT
        connected = _client.connect(
            clientId.c_str(),
            _username.c_str(),
            _password.c_str(),
            _lwtTopic.c_str(),
            1, // QoS 1 for LWT
            true, // retained
            lwtPayload.c_str()
        );
    } else {
        // Connect without authentication but with LWT
        connected = _client.connect(
            clientId.c_str(),
            _lwtTopic.c_str(),
            1,
            true,
            lwtPayload.c_str()
        );
    }

    if (connected) {
        Serial.println("✓ Connected");
        _reconnectRetries = 0;
        _isConnected = true;

        // Publish online status
        JsonDocument statusDoc;
        statusDoc["status"] = "online";
        statusDoc["timestamp"] = now / 1000;
        String statusPayload;
        serializeJson(statusDoc, statusPayload);
        publishStatus("", "online", statusDoc.as<JsonObject>());

        // Subscribe to command topic
        subscribe();
    } else {
        Serial.print("✗ Failed, rc=");
        Serial.println(_client.state());
        _reconnectRetries++;

        if (_reconnectRetries >= 10) {
            Serial.println("[MQTT] Max reconnection attempts reached, resetting counter");
            _reconnectRetries = 0;
        }
    }
}

unsigned long MQTTClient::getReconnectDelay() {
    // Exponential backoff: 1s, 2s, 4s, 8s, 16s, max 30s
    unsigned long delay = 1000 * (1 << _reconnectRetries);
    return min(delay, 30000UL);
}

void MQTTClient::subscribe() {
    if (_client.subscribe(_commandTopic.c_str(), 1)) { // QoS 1
        Serial.print("[MQTT] ✓ Subscribed to: ");
        Serial.println(_commandTopic);
    } else {
        Serial.print("[MQTT] ✗ Failed to subscribe to: ");
        Serial.println(_commandTopic);
    }
}

bool MQTTClient::publish(const char* topic, const char* payload, bool retained) {
    if (!_client.connected()) {
        Serial.println("[MQTT] ✗ Not connected, cannot publish");
        return false;
    }

    bool result = _client.publish(topic, payload, retained);

    if (result) {
        Serial.print("[MQTT] ✓ Published to: ");
        Serial.println(topic);
    } else {
        Serial.print("[MQTT] ✗ Failed to publish to: ");
        Serial.println(topic);
    }

    return result;
}

bool MQTTClient::publishStatus(const String& commandId, const String& status,
                                JsonObject result, const String& errorMsg) {
    JsonDocument doc;

    if (commandId.length() > 0) {
        doc["command_id"] = commandId;
    }

    doc["status"] = status;
    doc["timestamp"] = millis() / 1000;

    if (!result.isNull()) {
        doc["result"] = result;
    }

    if (errorMsg.length() > 0) {
        doc["error_message"] = errorMsg;
    }

    String payload;
    serializeJson(doc, payload);

    return publish(_statusTopic.c_str(), payload.c_str(), false);
}

bool MQTTClient::publishTelemetry(JsonObject telemetry) {
    JsonDocument doc;
    doc["timestamp"] = millis() / 1000;
    doc["data"] = telemetry;

    String payload;
    serializeJson(doc, payload);

    return publish(_telemetryTopic.c_str(), payload.c_str(), false);
}

bool MQTTClient::publishAttendance(const String& deviceId, const String& memberId,
                                    const String& memberName, uint16_t confidence,
                                    bool accessGranted) {
    JsonDocument doc;
    doc["event"] = accessGranted ? "check_in" : "access_denied";
    doc["device_id"] = deviceId;
    doc["member_id"] = memberId;
    doc["member_name"] = memberName;
    doc["confidence"] = confidence;
    doc["access_granted"] = accessGranted;

    // ISO 8601 timestamp
    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    char timestamp[30];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    doc["timestamp"] = timestamp;

    String payload;
    serializeJson(doc, payload);

    return publish(_attendanceTopic.c_str(), payload.c_str(), false);
}

void MQTTClient::messageCallback(char* topic, uint8_t* payload, unsigned int length) {
    if (instance) {
        instance->onMessage(topic, payload, length);
    }
}

void MQTTClient::onMessage(char* topic, uint8_t* payload, unsigned int length) {
    Serial.print("[MQTT] ← Message received on topic: ");
    Serial.println(topic);

    // Parse JSON payload
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.print("[MQTT] ✗ JSON parse error: ");
        Serial.println(error.c_str());
        return;
    }

    // Extract command fields
    String commandId = doc["command_id"] | "";
    String type = doc["type"] | "";
    JsonObject params = doc["params"].as<JsonObject>();

    if (commandId.length() == 0 || type.length() == 0) {
        Serial.println("[MQTT] ✗ Invalid command format (missing command_id or type)");
        return;
    }

    Serial.print("[MQTT] Command ID: ");
    Serial.println(commandId);
    Serial.print("[MQTT] Type: ");
    Serial.println(type);

    // Call command callback if set
    if (_commandCallback) {
        _commandCallback(commandId, type, params);
    } else {
        Serial.println("[MQTT] ⚠ No command callback registered");
    }
}

void MQTTClient::setCommandCallback(CommandCallback callback) {
    _commandCallback = callback;
}

bool MQTTClient::isConnected() {
    return _isConnected && _client.connected();
}

String MQTTClient::getCommandTopic() {
    return _commandTopic;
}

String MQTTClient::getStatusTopic() {
    return _statusTopic;
}

String MQTTClient::getTelemetryTopic() {
    return _telemetryTopic;
}

String MQTTClient::getAttendanceTopic() {
    return _attendanceTopic;
}
