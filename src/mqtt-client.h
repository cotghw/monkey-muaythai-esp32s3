#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "wifi-manager.h"

// MQTT callback function type
typedef std::function<void(const String& commandId, const String& type, JsonObject params)> CommandCallback;

class MQTTClient {
private:
    PubSubClient _client;
    WiFiClient _wifiClient;
    WiFiManager* _wifi;
    String _deviceId;
    String _broker;
    uint16_t _port;
    String _username;
    String _password;

    unsigned long _lastReconnect;
    uint8_t _reconnectRetries;
    bool _isConnected;

    CommandCallback _commandCallback;

    // Topic names
    String _commandTopic;
    String _statusTopic;
    String _telemetryTopic;
    String _lwtTopic;

    // Internal methods
    void onMessage(char* topic, uint8_t* payload, unsigned int length);
    void reconnect();
    unsigned long getReconnectDelay();
    void setupTopics();

    // Static callback wrapper for PubSubClient
    static void messageCallback(char* topic, uint8_t* payload, unsigned int length);
    static MQTTClient* instance;

public:
    MQTTClient(WiFiManager* wifi);

    bool begin(const char* broker, uint16_t port, const char* username,
               const char* password, const String& deviceId);

    void loop();
    void subscribe();
    bool publish(const char* topic, const char* payload, bool retained = false);
    bool publishStatus(const String& commandId, const String& status,
                      JsonObject result = JsonObject(), const String& errorMsg = "");
    bool publishTelemetry(JsonObject telemetry);

    void setCommandCallback(CommandCallback callback);

    bool isConnected();
    String getCommandTopic();
    String getStatusTopic();
    String getTelemetryTopic();
};

#endif
