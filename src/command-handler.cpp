#include "command-handler.h"

CommandHandler::CommandHandler(FingerprintHandler* fp, MQTTClient* mqtt) :
    _fp(fp),
    _mqtt(mqtt),
    _paused(false),
    _pauseStartTime(0)
{
}

bool CommandHandler::executeCommand(const String& commandId, const String& type,
                                    JsonObject params) {
    Serial.print("[CMD] Executing command: ");
    Serial.print(type);
    Serial.print(" (ID: ");
    Serial.print(commandId);
    Serial.println(")");

    // Pause auto-login mode when command arrives
    pause();

    CommandResult result = CMD_FAILED;

    // Route to appropriate handler
    if (type == "enroll") {
        result = handleEnroll(commandId, params);
    } else if (type == "delete") {
        result = handleDelete(commandId, params);
    } else if (type == "delete_all") {
        result = handleDeleteAll(commandId);
    } else if (type == "get_info") {
        result = handleGetInfo(commandId);
    } else if (type == "restart") {
        result = handleRestart(commandId);
    } else {
        Serial.print("[CMD] ✗ Unknown command type: ");
        Serial.println(type);
        publishStatus(commandId, "failed", JsonObject(),
                     "Unknown command type: " + type);
        resume();
        return false;
    }

    // Resume auto-login mode after command completes
    resume();

    return (result == CMD_SUCCESS);
}

CommandResult CommandHandler::handleEnroll(const String& cmdId, JsonObject params) {
    Serial.println("[CMD] → Starting fingerprint enrollment");

    // Extract parameters
    int fingerprintId = params["fingerprint_id"] | -1;
    String employeeCode = params["employee_code"] | "";

    if (fingerprintId < 1 || fingerprintId > 127) {
        Serial.println("[CMD] ✗ Invalid fingerprint_id (must be 1-127)");
        publishStatus(cmdId, "failed", JsonObject(),
                     "Invalid fingerprint_id (must be 1-127)");
        return CMD_INVALID_PARAMS;
    }

    Serial.print("[CMD] Fingerprint ID: ");
    Serial.println(fingerprintId);
    Serial.print("[CMD] Employee Code: ");
    Serial.println(employeeCode);

    // Update status to processing
    publishStatus(cmdId, "processing");

    // Enroll fingerprint
    int enrollResult = _fp->enrollFingerprint(fingerprintId);

    if (enrollResult < 0) {
        Serial.println("[CMD] ✗ Enrollment failed");
        publishStatus(cmdId, "failed", JsonObject(), "Fingerprint enrollment failed");
        return CMD_SENSOR_ERROR;
    }

    // Get template data
    uint8_t templateBuffer[512];
    uint16_t templateSize = 0;

    if (!_fp->getTemplate(fingerprintId, templateBuffer, &templateSize)) {
        Serial.println("[CMD] ✗ Failed to get template data");
        publishStatus(cmdId, "failed", JsonObject(), "Failed to get template data");
        return CMD_SENSOR_ERROR;
    }

    // Create result object
    JsonDocument resultDoc;
    resultDoc["fingerprint_id"] = fingerprintId;
    resultDoc["employee_code"] = employeeCode;
    resultDoc["template_size"] = templateSize;
    resultDoc["confidence"] = _fp->getConfidence();

    Serial.println("[CMD] ✓ Enrollment completed successfully");
    publishStatus(cmdId, "completed", resultDoc.as<JsonObject>());

    return CMD_SUCCESS;
}

CommandResult CommandHandler::handleDelete(const String& cmdId, JsonObject params) {
    Serial.println("[CMD] → Deleting fingerprint");

    int fingerprintId = params["fingerprint_id"] | -1;

    if (fingerprintId < 1 || fingerprintId > 127) {
        Serial.println("[CMD] ✗ Invalid fingerprint_id (must be 1-127)");
        publishStatus(cmdId, "failed", JsonObject(),
                     "Invalid fingerprint_id (must be 1-127)");
        return CMD_INVALID_PARAMS;
    }

    publishStatus(cmdId, "processing");

    if (!_fp->deleteFingerprint(fingerprintId)) {
        Serial.println("[CMD] ✗ Delete failed");
        publishStatus(cmdId, "failed", JsonObject(), "Failed to delete fingerprint");
        return CMD_SENSOR_ERROR;
    }

    JsonDocument resultDoc;
    resultDoc["fingerprint_id"] = fingerprintId;
    resultDoc["deleted"] = true;

    Serial.println("[CMD] ✓ Delete completed successfully");
    publishStatus(cmdId, "completed", resultDoc.as<JsonObject>());

    return CMD_SUCCESS;
}

CommandResult CommandHandler::handleDeleteAll(const String& cmdId) {
    Serial.println("[CMD] → Deleting all fingerprints");

    publishStatus(cmdId, "processing");

    if (!_fp->deleteAllFingerprints()) {
        Serial.println("[CMD] ✗ Delete all failed");
        publishStatus(cmdId, "failed", JsonObject(), "Failed to delete all fingerprints");
        return CMD_SENSOR_ERROR;
    }

    JsonDocument resultDoc;
    resultDoc["deleted_all"] = true;

    Serial.println("[CMD] ✓ Delete all completed successfully");
    publishStatus(cmdId, "completed", resultDoc.as<JsonObject>());

    return CMD_SUCCESS;
}

CommandResult CommandHandler::handleGetInfo(const String& cmdId) {
    Serial.println("[CMD] → Getting sensor info");

    publishStatus(cmdId, "processing");

    uint16_t templateCount = _fp->getTemplateCount();

    JsonDocument resultDoc;
    resultDoc["template_count"] = templateCount;
    resultDoc["sensor_type"] = "R307";
    resultDoc["max_capacity"] = 1000;

    Serial.println("[CMD] ✓ Get info completed successfully");
    publishStatus(cmdId, "completed", resultDoc.as<JsonObject>());

    return CMD_SUCCESS;
}

CommandResult CommandHandler::handleRestart(const String& cmdId) {
    Serial.println("[CMD] → Restarting device");

    publishStatus(cmdId, "processing");

    JsonDocument resultDoc;
    resultDoc["restarting"] = true;

    Serial.println("[CMD] ✓ Restart command accepted");
    publishStatus(cmdId, "completed", resultDoc.as<JsonObject>());

    delay(1000);
    ESP.restart();

    return CMD_SUCCESS;
}

void CommandHandler::publishStatus(const String& cmdId, const String& status,
                                   JsonObject result, const String& errorMsg) {
    if (_mqtt && _mqtt->isConnected()) {
        _mqtt->publishStatus(cmdId, status, result, errorMsg);
    }
}

JsonDocument CommandHandler::createResultDoc() {
    JsonDocument doc;
    return doc;
}

bool CommandHandler::isPaused() {
    return _paused;
}

void CommandHandler::pause() {
    if (!_paused) {
        _paused = true;
        _pauseStartTime = millis();
        Serial.println("[CMD] ⏸ Auto-login paused");
    }
}

void CommandHandler::resume() {
    if (_paused) {
        _paused = false;
        unsigned long pauseDuration = millis() - _pauseStartTime;
        Serial.print("[CMD] ▶ Auto-login resumed (paused for ");
        Serial.print(pauseDuration);
        Serial.println("ms)");
    }
}
