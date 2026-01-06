#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "fingerprint-handler.h"
#include "mqtt-client.h"

// Command execution results
enum CommandResult {
    CMD_SUCCESS,
    CMD_FAILED,
    CMD_INVALID_PARAMS,
    CMD_SENSOR_ERROR,
    CMD_TIMEOUT
};

class CommandHandler {
private:
    FingerprintHandler* _fp;
    MQTTClient* _mqtt;

    // Command handlers
    CommandResult handleEnroll(const String& cmdId, JsonObject params);
    CommandResult handleDelete(const String& cmdId, JsonObject params);
    CommandResult handleDeleteAll(const String& cmdId);
    CommandResult handleGetInfo(const String& cmdId);
    CommandResult handleRestart(const String& cmdId);

    // Helper methods
    void publishStatus(const String& cmdId, const String& status,
                      JsonObject result = JsonObject(), const String& errorMsg = "");
    JsonDocument createResultDoc();

public:
    CommandHandler(FingerprintHandler* fp, MQTTClient* mqtt);

    bool executeCommand(const String& commandId, const String& type,
                       JsonObject params);

    // Auto-login pause control
    bool isPaused();
    void pause();
    void resume();

private:
    bool _paused;
    unsigned long _pauseStartTime;
};

#endif
