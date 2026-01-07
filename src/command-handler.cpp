#include "command-handler.h"
#include <base64.h>

CommandHandler::CommandHandler(FingerprintHandler* fp, MQTTClient* mqtt,
                               DirectusClient* directus, WiFiManager* wifi) :
    _fp(fp),
    _mqtt(mqtt),
    _directus(directus),
    _wifi(wifi),
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
    } else if (type == "sync_all") {
        result = handleSyncAll(commandId);
    } else if (type == "get_status") {
        result = handleGetStatus(commandId);
    } else if (type == "update") {
        result = handleUpdate(commandId, params);
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
    String memberId = params["member_id"] | "";

    if (fingerprintId < 1 || fingerprintId > 127) {
        Serial.println("[CMD] ✗ Invalid fingerprint_id (must be 1-127)");
        publishStatus(cmdId, "failed", JsonObject(),
                     "Invalid fingerprint_id (must be 1-127)");
        return CMD_INVALID_PARAMS;
    }

    Serial.print("[CMD] Fingerprint ID: ");
    Serial.println(fingerprintId);
    Serial.print("[CMD] Member ID: ");
    Serial.println(memberId);

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

    // Upload to Directus
    String templateBase64 = base64::encode(templateBuffer, templateSize);
    String deviceMac = _wifi->getMACAddress();

    if (!_directus->enrollFingerprint(deviceMac, fingerprintId, templateBase64, memberId)) {
        Serial.println("[CMD] ⚠ Failed to upload to Directus (saved locally)");
    }

    // Create result object
    JsonDocument resultDoc;
    resultDoc["fingerprint_id"] = fingerprintId;
    resultDoc["member_id"] = memberId;
    resultDoc["template_size"] = templateSize;
    resultDoc["confidence"] = _fp->getConfidence();

    Serial.println("[CMD] ✓ Enrollment completed successfully");
    publishStatus(cmdId, "completed", resultDoc.as<JsonObject>());

    return CMD_SUCCESS;
}

CommandResult CommandHandler::handleUpdate(const String& cmdId, JsonObject params) {
    Serial.println("[CMD] → Starting fingerprint update (re-enrollment)");

    // Extract parameters
    int fingerprintId = params["fingerprint_id"] | -1;
    String memberId = params["member_id"] | "";

    if (fingerprintId < 1 || fingerprintId > 127) {
        Serial.println("[CMD] ✗ Invalid fingerprint_id (must be 1-127)");
        publishStatus(cmdId, "failed", JsonObject(),
                     "Invalid fingerprint_id (must be 1-127)");
        return CMD_INVALID_PARAMS;
    }

    Serial.print("[CMD] Update fingerprint ID: ");
    Serial.println(fingerprintId);
    Serial.print("[CMD] Member ID: ");
    Serial.println(memberId);

    // Update status to processing
    publishStatus(cmdId, "processing");

    // Step 1: Delete existing fingerprint at this ID
    Serial.println("[CMD] Step 1: Deleting existing fingerprint...");
    bool existed = _fp->deleteFingerprint(fingerprintId);
    if (existed) {
        Serial.println("[CMD] ✓ Existing fingerprint deleted");
    } else {
        Serial.println("[CMD] ⚠ No existing fingerprint at this ID (continuing anyway)");
    }

    // Small delay to ensure sensor is ready
    delay(100);

    // Step 2: Enroll new fingerprint
    Serial.println("[CMD] Step 2: Enrolling new fingerprint...");
    int enrollResult = _fp->enrollFingerprint(fingerprintId);

    if (enrollResult < 0) {
        Serial.println("[CMD] ✗ Enrollment failed");
        publishStatus(cmdId, "failed", JsonObject(), "Fingerprint re-enrollment failed");
        return CMD_SENSOR_ERROR;
    }

    // Step 3: Get template data
    uint8_t templateBuffer[512];
    uint16_t templateSize = 0;

    if (!_fp->getTemplate(fingerprintId, templateBuffer, &templateSize)) {
        Serial.println("[CMD] ✗ Failed to get template data");
        publishStatus(cmdId, "failed", JsonObject(), "Failed to get template data");
        return CMD_SENSOR_ERROR;
    }

    // Step 4: Upload to Directus
    String templateBase64 = base64::encode(templateBuffer, templateSize);
    String deviceMac = _wifi->getMACAddress();

    bool synced = _directus->enrollFingerprint(deviceMac, fingerprintId, templateBase64, memberId);
    if (!synced) {
        Serial.println("[CMD] ⚠ Failed to upload to Directus (saved locally)");
        // Continue - fingerprint is enrolled on device
    }

    // Create result object
    JsonDocument resultDoc;
    resultDoc["fingerprint_id"] = fingerprintId;
    resultDoc["member_id"] = memberId;
    resultDoc["template_size"] = templateSize;
    resultDoc["confidence"] = _fp->getConfidence();
    resultDoc["updated"] = true;  // Flag to indicate this was an update
    resultDoc["synced"] = synced; // Directus sync status

    Serial.println("[CMD] ✓ Update completed successfully");
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

CommandResult CommandHandler::handleSyncAll(const String& cmdId) {
    Serial.println("[CMD] → Syncing all fingerprints from Directus");
    publishStatus(cmdId, "processing");

    // Get device MAC
    String deviceMac = _wifi->getMACAddress();

    // Fetch fingerprints from Directus
    JsonDocument doc;
    int count = _directus->getFingerprints(deviceMac, doc);

    if (count == 0) {
        JsonDocument resultDoc;
        resultDoc["total_fingerprints"] = 0;
        resultDoc["synced"] = 0;
        resultDoc["failed"] = 0;
        publishStatus(cmdId, "completed", resultDoc.as<JsonObject>());
        return CMD_SUCCESS;
    }

    // Download and upload each template
    JsonArray data = doc["data"];
    int synced = 0, failed = 0;

    for (int i = 0; i < count; i++) {
        String fpId = data[i]["id"].as<String>();
        uint8_t localId;
        uint8_t templateBuffer[512];
        uint16_t templateSize;

        if (_directus->downloadFingerprintTemplate(fpId, templateBuffer, &templateSize, &localId)) {
            if (_fp->uploadModel(localId, templateBuffer, templateSize)) {
                synced++;
                Serial.printf("[CMD] ✓ Synced fingerprint ID %d\n", localId);
            } else {
                failed++;
                Serial.printf("[CMD] ✗ Failed to upload ID %d to sensor\n", localId);
            }
        } else {
            failed++;
            Serial.printf("[CMD] ✗ Failed to download template %s\n", fpId.c_str());
        }
        delay(100);  // Prevent overwhelming sensor
    }

    JsonDocument resultDoc;
    resultDoc["total_fingerprints"] = count;
    resultDoc["synced"] = synced;
    resultDoc["failed"] = failed;

    Serial.printf("[CMD] ✓ Sync completed: %d/%d synced\n", synced, count);
    publishStatus(cmdId, "completed", resultDoc.as<JsonObject>());
    return CMD_SUCCESS;
}

CommandResult CommandHandler::handleGetStatus(const String& cmdId) {
    Serial.println("[CMD] → Getting device status");

    JsonDocument resultDoc;
    resultDoc["device_mac"] = _wifi->getMACAddress();
    resultDoc["wifi_connected"] = _wifi->isConnected();
    resultDoc["wifi_rssi"] = WiFi.RSSI();
    resultDoc["mqtt_connected"] = _mqtt->isConnected();
    resultDoc["sensor_ok"] = (_fp->getTemplateCount() >= 0);
    resultDoc["template_count"] = _fp->getTemplateCount();
    resultDoc["free_heap"] = ESP.getFreeHeap();
    resultDoc["uptime_seconds"] = millis() / 1000;

    Serial.println("[CMD] ✓ Status retrieved");
    publishStatus(cmdId, "completed", resultDoc.as<JsonObject>());
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
