# MQTT Remote Control Implementation Plan

**Date:** 2025-12-24
**Issue:** Remote device management via MQTT
**Status:** In Progress (60% complete - ESP32 implementation done, needs fixes)
**Updated:** 2025-12-26 - Code review completed, critical issues identified
**Review Report:** /Users/yihan/testing-base/esp32s3/plans/reports/code-reviewer-251226-mqtt-implementation.md

## Overview

Implement real-time remote control for ESP32 fingerprint devices using MQTT protocol. Allows admin to manage fingerprints remotely via web interface without serial access. Uses MQTT broker as message transport layer between Directus backend and ESP32 devices.

## Implementation Summary (2025-12-26)

### ✅ Completed Components

**ESP32 Firmware:**
- ✅ `src/mqtt-client.h/cpp` - Full MQTT client with auto-reconnect, LWT, QoS support
- ✅ `src/command-handler.h/cpp` - All 5 commands (enroll, delete, delete_all, get_info, restart)
- ✅ `src/main.cpp` - Integrated MQTT loop with existing fingerprint system
- ✅ `src/config.h` - MQTT broker configuration added
- ✅ `platformio.ini` - PubSubClient library dependency
- ✅ **Code compiles successfully** - Ready to flash

**Backend Services (Documented):**
- ✅ `docs/MQTT-REMOTE-CONTROL-SETUP.md` - Complete setup guide
- ✅ MQTT Publisher Service (Node.js) - Polls Directus → publishes commands
- ✅ MQTT Subscriber Service (Node.js) - Listens status → updates DB
- ✅ Directus `device_commands` collection schema
- ✅ Testing instructions (MQTTX, mosquitto_pub/sub)

**Code Review:**
- ✅ Review completed: 7/10 quality score
- ✅ Report: `/plans/reports/code-reviewer-251226-mqtt-implementation.md`
- ⚠️ 4 critical issues identified (buffer overflow, credentials, memory leaks, timeouts)
- ⚠️ 4 high-priority issues (QoS, JSON allocation, timestamps, buffer size)

### ⚠️ Pending Work

**Critical Fixes Needed:**
1. Add MQTT message size validation (4KB limit)
2. Move credentials to config.example.h + .gitignore
3. Fix memory leaks (use stack allocation)
4. Add enrollment timeout protection (60s max)
5. Configure PubSubClient buffer size (4096 bytes)
6. Use StaticJsonDocument instead of JsonDocument

**Not Yet Implemented:**
- Security layer (MQTT TLS, ACL configuration)
- Automated testing suite
- Production broker setup (currently using public test broker)
- Directus backend service deployment
- Load testing & performance optimization

### 📊 Progress: 60% Complete
- **ESP32 Core:** 100% (needs hardening)
- **Documentation:** 100%
- **Backend Services:** 0% (code provided, needs deployment)
- **Security:** 0% (plan exists, not implemented)
- **Testing:** 0% (manual instructions only)

## Why MQTT instead of WebSocket?

**Advantages of MQTT:**
- ✅ **Lightweight protocol**: Lower overhead, better for IoT devices with limited resources
- ✅ **QoS levels**: Guaranteed message delivery (QoS 1 for commands)
- ✅ **Topic-based isolation**: Natural multi-device support via topic filtering
- ✅ **Built-in features**: Last Will & Testament (LWT), retained messages, keepalive
- ✅ **Message queuing**: Broker queues messages when device offline
- ✅ **Standard protocol**: Wide industry adoption, many managed services available
- ✅ **Reduced complexity**: No need for custom WebSocket subscription management

**WebSocket limitations:**
- ⚠️ Higher protocol overhead for IoT use cases
- ⚠️ Requires custom message queuing implementation
- ⚠️ No built-in QoS or delivery guarantees
- ⚠️ More complex multi-device filtering logic

## Goals

1. Enable remote fingerprint enrollment/deletion via admin web interface
2. Real-time command delivery (<100ms latency)
3. Bi-directional status updates (device → admin)
4. Multi-device support with device-specific filtering
5. Production-ready with auto-reconnect and error handling

## Architecture

```
Admin Web UI → Directus API → MQTT Broker (HiveMQ/Mosquitto/CloudMQTT)
                                    ↓
                      ESP32 Device (subscribed to device/{device_id}/commands)
                                    ↓
                         Execute Command → Publish to device/{device_id}/status
                                    ↓
                              Directus Flow (webhook) → Update DB
```

**MQTT Topics Structure:**
- `device/{device_id}/commands` - Commands from Directus to ESP32
- `device/{device_id}/status` - Status updates from ESP32 to Directus
- `device/{device_id}/telemetry` - Optional: Device metrics, uptime, etc.
- `system/broadcast` - Optional: System-wide announcements

## Implementation Phases

### Phase 1: Directus Schema Setup

**Collection: `device_commands`**
```json
{
  "id": "UUID",
  "device_id": "UUID",           // FK to devices
  "command_type": "enum",        // enroll, delete, delete_all, get_info, restart
  "fingerprint_id": "int",       // Optional, for delete command
  "employee_code": "string",     // Optional, for enroll command
  "status": "enum",              // pending, processing, completed, failed
  "result": "json",              // Command execution result
  "error_message": "text",       // If failed
  "created_at": "timestamp",
  "executed_at": "timestamp",
  "created_by": "UUID"           // FK to directus_users
}
```

**Enums:**
- `command_type`: enroll | delete | delete_all | get_info | restart
- `status`: pending | processing | completed | failed

**Relations:**
- device_id → devices (M2O)
- created_by → directus_users (M2O)

**Indexes:**
- `device_id + status` (for filtering pending commands)
- `created_at` (for cleanup old commands)

### Phase 2: ESP32 MQTT Client

**Files to create/modify:**
```
src/
├── mqtt-client.h               // NEW: MQTT client wrapper class
├── mqtt-client.cpp             // NEW: MQTT implementation
├── command-handler.h           // NEW: Command execution logic
├── command-handler.cpp         // NEW: Command handlers
├── config.h                    // MODIFY: Add MQTT broker config
└── main.cpp                    // MODIFY: Initialize MQTT client
```

**Dependencies:**
```ini
# platformio.ini
lib_deps =
    Adafruit Fingerprint Sensor Library @ ^2.1.3
    ArduinoJson @ ^7.4.2
    knolleary/PubSubClient @ ^2.8   # NEW: MQTT client library
```

**MQTT Client Interface:**
```cpp
class MQTTClient {
public:
    MQTTClient(WiFiManager* wifi);
    bool begin(const char* broker, uint16_t port, const char* username,
               const char* password, const String& deviceId);
    void loop();
    void subscribe();
    bool publish(const char* topic, const char* payload, bool retained = false);

private:
    void onMessage(char* topic, uint8_t* payload, unsigned int length);
    void reconnect();
    String getCommandTopic();
    String getStatusTopic();
    String getTelemetryTopic();

    PubSubClient _client;
    WiFiClient _wifiClient;
    WiFiManager* _wifi;
    String _deviceId;
    unsigned long _lastReconnect;
    uint8_t _reconnectRetries;
};
```

**Command Handler Interface:**
```cpp
class CommandHandler {
public:
    CommandHandler(FingerprintHandler* fp, MQTTClient* mqtt);

    bool executeCommand(const String& commandId, const String& type,
                       JsonObject params);

private:
    bool handleEnroll(const String& cmdId, JsonObject params);
    bool handleDelete(const String& cmdId, JsonObject params);
    bool handleDeleteAll(const String& cmdId);
    bool handleGetInfo(const String& cmdId);
    bool handleRestart(const String& cmdId);

    void publishStatus(const String& cmdId, const String& status,
                      JsonObject result);

    FingerprintHandler* _fp;
    MQTTClient* _mqtt;
};
```

**MQTT Message Formats:**

**Command message (received on `device/{device_id}/commands`):**
```json
{
  "command_id": "uuid-1234",
  "type": "enroll",
  "params": {
    "fingerprint_id": 5,
    "employee_code": "EMP001"
  },
  "timestamp": 1234567890
}
```

**Status message (published to `device/{device_id}/status`):**
```json
{
  "command_id": "uuid-1234",
  "status": "completed",
  "result": {
    "fingerprint_id": 5,
    "confidence": 142,
    "template_size": 512
  },
  "error_message": null,
  "timestamp": 1234567891
}
```

### Phase 3: Directus MQTT Integration

**Backend Components:**

1. **MQTT Publisher Service (Node.js/Python):**
   - Watches `device_commands` collection for new pending commands
   - Publishes command to MQTT topic `device/{device_id}/commands`
   - Updates command status to "processing"

2. **MQTT Subscriber Service:**
   - Subscribes to `device/+/status` (all device status updates)
   - Parses incoming status messages
   - Updates `device_commands` collection with results

3. **Directus Flows:**
   - **Flow 1**: Trigger on `device_commands.create` → Call MQTT Publisher API
   - **Flow 2**: Webhook endpoint → Receives MQTT status → Update command

**MQTT Publisher API Endpoint:**
```
POST /api/mqtt/publish-command
{
  "command_id": "uuid-1234",
  "device_id": "uuid-5678",
  "type": "enroll",
  "params": {...}
}
```

**Alternative: Directus Extension (Custom Endpoint):**
```js
// extensions/endpoints/mqtt-commands/index.js
export default {
  id: 'mqtt-commands',
  handler: (router, { services, database }) => {
    router.post('/publish', async (req, res) => {
      const mqttClient = getMQTTClient();
      const { device_id, command } = req.body;

      await mqttClient.publish(
        `device/${device_id}/commands`,
        JSON.stringify(command)
      );

      res.json({ success: true });
    });
  }
};
```

### Phase 4: Connection Management

**Features:**
- Auto-reconnect on disconnect (exponential backoff)
- MQTT keepalive: 60s (broker will detect dead connections)
- Last Will & Testament (LWT) for device offline detection
- QoS 1 for commands (at least once delivery)
- QoS 0 for telemetry (fire and forget)
- Graceful degradation if MQTT broker unavailable

**MQTT Connection Parameters:**
```cpp
// config.h
#define MQTT_BROKER "broker.hivemq.com"  // or Mosquitto/CloudMQTT
#define MQTT_PORT 1883                    // or 8883 for TLS
#define MQTT_USERNAME "esp32_device"      // optional
#define MQTT_PASSWORD "secure_password"   // optional
#define MQTT_KEEPALIVE 60                 // seconds
```

**Reconnect Strategy:**
```cpp
// Exponential backoff: 1s, 2s, 4s, 8s, 16s, max 30s
unsigned long reconnectDelay = min(1000 * pow(2, retries), 30000);
```

**Last Will & Testament (LWT):**
```cpp
// Published to device/{device_id}/status when device disconnects
{
  "status": "offline",
  "timestamp": 1234567890
}
```

### Phase 5: Error Handling

**Error Scenarios:**
1. MQTT broker unreachable → Retry with exponential backoff
2. Command parsing error → Publish status: failed with error details
3. Fingerprint operation failed → Publish error message to status topic
4. Network timeout → Auto-reconnect to MQTT broker
5. Invalid command type → Log error, publish failed status
6. QoS 1 message not acknowledged → MQTT library auto-retries
7. Broker authentication failed → Check credentials, log error

**Logging:**
```cpp
Serial.println("[MQTT] Connected to broker: broker.hivemq.com:1883");
Serial.println("[MQTT] Subscribed to: device/AA-BB-CC-DD-EE-FF/commands");
Serial.println("[CMD] Received: enroll fingerprint #5");
Serial.println("[CMD] ✓ Completed: enrolled #5, publishing status...");
Serial.println("[MQTT] Published to: device/AA-BB-CC-DD-EE-FF/status");
Serial.println("[MQTT] ✗ Connection lost, reconnecting in 2s...");
```

### Phase 6: Testing Strategy

**Unit Tests:**
- [ ] MQTT topic name generation (device-specific)
- [ ] Command JSON parsing
- [ ] Status message payload generation
- [ ] QoS level validation

**Integration Tests:**
- [ ] End-to-end: Publish command via MQTT → ESP32 execute → Status update
- [ ] Multi-device: Ensure device only receives own commands (topic filtering)
- [ ] Reconnection: Kill MQTT connection, verify auto-reconnect
- [ ] Error handling: Invalid command, network timeout, malformed JSON
- [ ] LWT: Disconnect ESP32, verify offline status published

**Testing Tools:**
- **MQTT.fx / MQTTX**: Desktop MQTT client for manual testing
- **mosquitto_pub/sub**: CLI tools for command-line testing
- **HiveMQ Web Client**: Browser-based MQTT testing

**Test Commands:**
```bash
# Test command publishing
mosquitto_pub -h broker.hivemq.com -t "device/AA-BB-CC-DD-EE-FF/commands" \
  -m '{"command_id":"test-1","type":"get_info","params":{}}'

# Monitor status updates
mosquitto_sub -h broker.hivemq.com -t "device/+/status"
```

**Load Tests:**
- [ ] 10 commands/second per device
- [ ] 100 devices connected simultaneously
- [ ] MQTT broker message throughput testing

### Phase 7: Security Considerations

**Authentication:**
- MQTT broker authentication via username/password
- Consider using client certificates (TLS mutual auth) for production
- Per-device credentials (unique username/password per ESP32)

**Transport Security:**
- Use MQTT over TLS (port 8883) instead of plain TCP (1883)
- Verify broker SSL certificate
- ESP32 TLS support via `WiFiClientSecure`

**Authorization (MQTT ACL):**
- Device can only subscribe to `device/{its_device_id}/#`
- Device can only publish to `device/{its_device_id}/status`
- Backend service has wildcard access to all topics
- Prevent cross-device command injection

**Data Validation:**
- Validate fingerprint_id range (1-127)
- Sanitize employee_code input
- Prevent command injection in JSON params
- Validate command_id format (UUID)

**Recommended MQTT Brokers with ACL:**
- **HiveMQ**: Enterprise-grade with fine-grained ACL
- **Mosquitto**: Open-source with ACL config file
- **AWS IoT Core**: Managed service with policy-based auth
- **CloudMQTT**: Hosted Mosquitto with web UI

## Implementation Order

1. **Day 1: MQTT Broker & Dependencies**
   - Set up MQTT broker (HiveMQ Cloud / Mosquitto / local Docker)
   - Create `device_commands` collection in Directus
   - Add PubSubClient library to platformio.ini
   - Test MQTT connection with mosquitto_pub/sub

2. **Day 2: ESP32 MQTT Client**
   - Implement `MQTTClient` class
   - Subscribe to device-specific command topic
   - Implement reconnection logic with exponential backoff
   - Test with MQTTX client

3. **Day 3: Command Handlers**
   - Implement `CommandHandler` class
   - Wire up enroll/delete/delete_all handlers
   - Publish status updates to MQTT status topic
   - Test end-to-end command flow

4. **Day 4: Directus Integration**
   - Create MQTT publisher service (Node.js/Python)
   - Set up Directus Flow to trigger on command creation
   - Create MQTT subscriber to update command status
   - Test via Directus Data Studio

5. **Day 5: Security & Production**
   - Enable MQTT TLS (port 8883)
   - Configure MQTT ACL for device isolation
   - Add LWT for offline detection
   - Load testing & documentation

## Success Criteria

- [ ] ESP32 receives commands within 100ms of MQTT publish
- [ ] Command status updates reflected in Directus real-time
- [ ] Auto-reconnect works after network/broker loss
- [ ] Zero false positives (topic isolation prevents cross-device commands)
- [ ] System handles 10+ commands/minute per device
- [ ] Graceful degradation when MQTT broker unavailable
- [ ] LWT properly detects device offline within keepalive timeout
- [ ] QoS 1 ensures command delivery (no message loss)

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| MQTT broker single point of failure | Critical | Use managed MQTT service (HiveMQ Cloud/AWS IoT) with HA |
| Multiple devices receive same command | Critical | Topic-based isolation: device/{device_id}/commands |
| Message delivery failure (QoS 0) | Medium | Use QoS 1 for commands, QoS 0 only for telemetry |
| ESP32 memory exhaustion | High | Use streaming JSON parser, limit message size (max 4KB) |
| MQTT broker rate limiting | Medium | Implement client-side throttling, respect broker limits |
| Network latency spikes | Low | MQTT keepalive detection, auto-reconnect on timeout |
| Unauthorized command injection | Critical | MQTT ACL + TLS authentication per device |

## Future Enhancements

- [ ] Batch command support (enroll multiple fingerprints)
- [ ] Command scheduling (enroll at specific time)
- [ ] Firmware OTA updates via MQTT (chunked binary transfer)
- [ ] Device telemetry streaming via `device/{device_id}/telemetry` topic
- [ ] Mobile app push notifications on command completion
- [ ] MQTT retained messages for device configuration
- [ ] Bridge multiple MQTT brokers for geo-distributed devices
- [ ] MQTT v5 support (reason codes, user properties)

## Dependencies

**Hardware:**
- ESP32-S3 with WiFi
- R307 Fingerprint Sensor

**Software:**
- Directus >= 10.x
- PlatformIO
- PubSubClient library ^2.8 (MQTT client for ESP32)
- ArduinoJson ^7.4.2

**MQTT Broker (choose one):**
- **HiveMQ Cloud** (free tier: 100 connections, managed)
- **Mosquitto** (self-hosted, open-source)
- **AWS IoT Core** (enterprise, pay-per-use)
- **CloudMQTT** (managed Mosquitto)
- **EMQX** (scalable, open-source)

**Backend Services:**
- MQTT Publisher service (Node.js/Python) - publishes commands
- MQTT Subscriber service (Node.js/Python) - listens for status updates
- Directus instance with Flows enabled
- Admin web interface (to be built)

## Notes

- **MQTT vs WebSocket**: MQTT is more lightweight, better for IoT devices with limited resources
- MQTT QoS levels ensure reliable message delivery (QoS 1 for commands)
- Topic-based pub/sub is more scalable than WebSocket subscriptions
- MQTT brokers handle connection management, message queuing automatically
- Consider rate limiting on admin UI to prevent command spam
- HiveMQ Cloud free tier suitable for development/testing (100 devices)
- For production, consider AWS IoT Core for enterprise features (device shadow, rules engine)

## Code Review Summary (2025-12-26)

**Overall Quality:** 7/10 - Good architecture, needs critical fixes

### Critical Issues to Fix Immediately
1. ⚠️ **Memory leaks** - Global objects allocated with `new` never freed
2. ⚠️ **MQTT buffer overflow** - No message size validation (4KB limit not enforced)
3. ⚠️ **Credentials exposed** - Hardcoded WiFi/Directus/MQTT credentials in config.h
4. ⚠️ **Command timeouts** - Enrollment can block indefinitely waiting for finger

### Implementation Status
- ✅ **Phase 2 Complete:** ESP32 MQTT client implemented (mqtt-client.h/cpp)
- ✅ **Phase 2 Complete:** Command handler with all 5 commands (enroll, delete, delete_all, get_info, restart)
- ✅ **Phase 4 Partial:** Auto-reconnect with exponential backoff works
- ✅ **Phase 4 Complete:** Last Will & Testament (LWT) implemented
- ⚠️ **Phase 3 Missing:** Directus backend services (MQTT publisher/subscriber) not implemented
- ⚠️ **Phase 5 Partial:** Basic error handling, missing timeout protection
- ❌ **Phase 6 Missing:** No tests found
- ❌ **Phase 7 Missing:** Security not implemented (TLS, ACL, credential protection)

### Next Steps (Priority Order)
1. Fix critical buffer overflow - add message size validation
2. Move credentials to .gitignore, rotate exposed secrets
3. Add command timeout protection (60s max)
4. Configure MQTT buffer size to 4096 bytes
5. Implement Directus MQTT publisher/subscriber services
6. Add unit tests for command parsing and MQTT handling
7. Implement TLS and MQTT ACL for production security

**Full Review:** See plans/reports/code-reviewer-251226-mqtt-implementation.md

## Resolved Questions

1. **Command queuing:** Both - ESP32 local queue + Directus as source of truth
2. **Command retention:** No auto-cleanup (keep all for audit trail)
3. **Failed commands:** Mark as failed status in Directus, no auto-retry (manual intervention required)
4. **Enrollment during auto-login:** ✅ IMPLEMENTED - pause auto-login when command arrives, resume after completion
5. **Device online status:** ✅ IMPLEMENTED - MQTT LWT for offline detection
6. **MQTT broker choice:** Start with HiveMQ Cloud (free tier), migrate to AWS IoT Core if needed
7. **QoS level for commands:** ⚠️ PARTIAL - QoS 1 subscribe works, publish needs fix (PubSubClient limitation)
8. **Message size limit:** ⚠️ NOT ENFORCED - Max 4KB per message defined but not validated in code

---

## Implementation Status by Phase (2025-12-26)

### Phase 1: Directus Schema ✅ **DOCUMENTED**
- Collection schema in `docs/MQTT-REMOTE-CONTROL-SETUP.md`
- Ready to create in Directus

### Phase 2: ESP32 MQTT Client ✅ **COMPLETE**
- `src/mqtt-client.h/cpp` created
- Auto-reconnect, LWT, topic management ✅
- ⚠️ Needs: Buffer validation, QoS enforcement

### Phase 3: Command Handlers ✅ **COMPLETE**  
- `src/command-handler.h/cpp` created
- All 5 commands implemented ✅
- ⚠️ Needs: Timeout protection

### Phase 4: Directus Integration ✅ **DOCUMENTED**
- Publisher/Subscriber services code provided
- ⚠️ Not deployed yet

### Phase 5: Connection Mgmt ✅ **IMPLEMENTED**
- Auto-reconnect ✅ | Keepalive ✅ | LWT ✅

### Phase 6: Error Handling ⚠️ **PARTIAL**
- Basic error handling ✅
- ⚠️ Missing message size validation, timeouts

### Phase 7: Testing ❌ **NOT DONE**
- Manual test instructions only

### Phase 8: Security ❌ **NOT DONE**
- ⚠️ Credentials exposed in config.h

---

## Files Created (2025-12-26)

**ESP32 Source:**
- `src/mqtt-client.h` (66 lines)
- `src/mqtt-client.cpp` (267 lines)
- `src/command-handler.h` (41 lines)
- `src/command-handler.cpp` (207 lines)
- `src/config.h` (modified - MQTT config added)
- `src/main.cpp` (modified - MQTT integration)

**Documentation:**
- `docs/MQTT-REMOTE-CONTROL-SETUP.md` (complete setup guide)
- `plans/reports/code-reviewer-251226-mqtt-implementation.md` (review report)

**Build Status:** ✅ Code compiles successfully

---

## Immediate Next Steps

1. **Before Testing:** Fix 4 critical issues from code review
2. **Before Production:** Deploy backend services, enable TLS/ACL
3. **For V2:** Automated tests, load testing, telemetry streaming
