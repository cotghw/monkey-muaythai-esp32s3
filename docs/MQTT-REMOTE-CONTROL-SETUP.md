# MQTT Remote Control Setup Guide

## Overview

MQTT-based remote control system cho phép quản lý ESP32 fingerprint devices từ xa qua Directus CMS. System sử dụng MQTT broker làm message transport layer.

## Architecture

```
Admin Web UI → Directus API → MQTT Broker (HiveMQ/Mosquitto)
                                    ↓
                      ESP32 Device (subscribed to device/{device_id}/commands)
                                    ↓
                         Execute Command → Publish to device/{device_id}/status
                                    ↓
                              MQTT Subscriber → Update Directus DB
```

## 1. MQTT Broker Setup

### Option 1: HiveMQ Cloud (Recommended for Development)

1. Đăng ký free account tại https://console.hivemq.cloud
2. Tạo cluster mới (100 connections miễn phí)
3. Note lại:
   - Cluster URL (VD: `abc123.s1.eu.hivemq.cloud`)
   - Port: `8883` (TLS) hoặc `1883` (TCP)
   - Username và Password

4. Cập nhật `src/config.h`:
```cpp
#define MQTT_BROKER "abc123.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883  // hoặc 1883 nếu dùng TCP
#define MQTT_USERNAME "your-username"
#define MQTT_PASSWORD "your-password"
```

### Option 2: Mosquitto (Self-hosted)

1. Install Mosquitto:
```bash
# macOS
brew install mosquitto

# Ubuntu/Debian
sudo apt-get install mosquitto mosquitto-clients

# Docker
docker run -d -p 1883:1883 -p 9001:9001 eclipse-mosquitto
```

2. Cấu hình (optional):
```bash
# mosquitto.conf
listener 1883
allow_anonymous true

# Với authentication
listener 1883
password_file /etc/mosquitto/passwd
```

3. Tạo user (nếu dùng auth):
```bash
mosquitto_passwd -c /etc/mosquitto/passwd esp32_device
```

4. Start broker:
```bash
# macOS
brew services start mosquitto

# Ubuntu
sudo systemctl start mosquitto

# Docker
docker start mosquitto-container
```

## 2. Directus Schema Setup

### Collection: `device_commands`

Tạo collection mới trong Directus với schema sau:

```json
{
  "collection": "device_commands",
  "fields": [
    {
      "field": "id",
      "type": "uuid",
      "schema": {
        "is_primary_key": true
      }
    },
    {
      "field": "device_id",
      "type": "uuid",
      "schema": {
        "is_nullable": false
      },
      "meta": {
        "interface": "select-dropdown-m2o",
        "display": "related-values"
      }
    },
    {
      "field": "command_type",
      "type": "string",
      "schema": {
        "is_nullable": false
      },
      "meta": {
        "interface": "select-dropdown",
        "options": {
          "choices": [
            {"text": "Enroll", "value": "enroll"},
            {"text": "Delete", "value": "delete"},
            {"text": "Delete All", "value": "delete_all"},
            {"text": "Get Info", "value": "get_info"},
            {"text": "Restart", "value": "restart"}
          ]
        }
      }
    },
    {
      "field": "fingerprint_id",
      "type": "integer",
      "schema": {
        "is_nullable": true
      },
      "meta": {
        "note": "Optional: For enroll/delete commands (1-127)"
      }
    },
    {
      "field": "employee_code",
      "type": "string",
      "schema": {
        "is_nullable": true
      },
      "meta": {
        "note": "Optional: For enroll command"
      }
    },
    {
      "field": "status",
      "type": "string",
      "schema": {
        "default_value": "pending",
        "is_nullable": false
      },
      "meta": {
        "interface": "select-dropdown",
        "options": {
          "choices": [
            {"text": "Pending", "value": "pending"},
            {"text": "Processing", "value": "processing"},
            {"text": "Completed", "value": "completed"},
            {"text": "Failed", "value": "failed"}
          ]
        }
      }
    },
    {
      "field": "result",
      "type": "json",
      "schema": {
        "is_nullable": true
      }
    },
    {
      "field": "error_message",
      "type": "text",
      "schema": {
        "is_nullable": true
      }
    },
    {
      "field": "created_at",
      "type": "timestamp",
      "schema": {
        "default_value": "CURRENT_TIMESTAMP"
      }
    },
    {
      "field": "executed_at",
      "type": "timestamp",
      "schema": {
        "is_nullable": true
      }
    },
    {
      "field": "created_by",
      "type": "uuid",
      "schema": {
        "is_nullable": true
      }
    }
  ]
}
```

### Relations

Tạo relationships:
- `device_id` → `devices.id` (Many-to-One)
- `created_by` → `directus_users.id` (Many-to-One)

### Indexes

Tạo indexes để tối ưu performance:
```sql
CREATE INDEX idx_device_commands_device_status ON device_commands(device_id, status);
CREATE INDEX idx_device_commands_created_at ON device_commands(created_at);
```

## 3. Backend Services Setup

Có 2 services cần chạy:
1. **MQTT Publisher** - Publish commands to MQTT
2. **MQTT Subscriber** - Listen for status updates from devices

### Service 1: MQTT Publisher (Node.js)

Tạo file `mqtt-publisher-service.js`:

```javascript
const mqtt = require('mqtt');
const axios = require('axios');

// Directus configuration
const DIRECTUS_URL = process.env.DIRECTUS_URL || 'http://localhost:8055';
const DIRECTUS_TOKEN = process.env.DIRECTUS_TOKEN;

// MQTT configuration
const MQTT_BROKER = process.env.MQTT_BROKER || 'broker.hivemq.com';
const MQTT_PORT = parseInt(process.env.MQTT_PORT || '1883');
const MQTT_USERNAME = process.env.MQTT_USERNAME || '';
const MQTT_PASSWORD = process.env.MQTT_PASSWORD || '';

// Connect to MQTT broker
const mqttClient = mqtt.connect(`mqtt://${MQTT_BROKER}:${MQTT_PORT}`, {
  username: MQTT_USERNAME,
  password: MQTT_PASSWORD,
  clientId: 'directus-publisher-' + Math.random().toString(16).substr(2, 8)
});

mqttClient.on('connect', () => {
  console.log('✓ Connected to MQTT broker:', MQTT_BROKER);
  startPolling();
});

mqttClient.on('error', (err) => {
  console.error('✗ MQTT error:', err);
});

// Poll for pending commands
async function startPolling() {
  console.log('→ Starting command polling...');

  setInterval(async () => {
    try {
      // Query pending commands
      const response = await axios.get(`${DIRECTUS_URL}/items/device_commands`, {
        headers: {
          'Authorization': `Bearer ${DIRECTUS_TOKEN}`
        },
        params: {
          filter: {
            status: { _eq: 'pending' }
          },
          limit: 10
        }
      });

      const commands = response.data.data;

      for (const command of commands) {
        await publishCommand(command);
      }
    } catch (error) {
      console.error('✗ Error polling commands:', error.message);
    }
  }, 2000); // Poll every 2 seconds
}

// Publish command to MQTT
async function publishCommand(command) {
  try {
    // Get device info
    const deviceResponse = await axios.get(
      `${DIRECTUS_URL}/items/devices/${command.device_id}`,
      {
        headers: { 'Authorization': `Bearer ${DIRECTUS_TOKEN}` }
      }
    );

    const device = deviceResponse.data.data;
    const deviceMac = device.mac_address;

    // Build MQTT topic
    const topic = `device/${deviceMac}/commands`;

    // Build payload
    const payload = {
      command_id: command.id,
      type: command.command_type,
      params: {
        fingerprint_id: command.fingerprint_id,
        employee_code: command.employee_code
      },
      timestamp: Math.floor(Date.now() / 1000)
    };

    // Publish to MQTT
    mqttClient.publish(topic, JSON.stringify(payload), { qos: 1 }, async (err) => {
      if (err) {
        console.error(`✗ Failed to publish command ${command.id}:`, err);
        return;
      }

      console.log(`✓ Published command ${command.id} to ${topic}`);

      // Update status to processing
      await axios.patch(
        `${DIRECTUS_URL}/items/device_commands/${command.id}`,
        { status: 'processing' },
        {
          headers: { 'Authorization': `Bearer ${DIRECTUS_TOKEN}` }
        }
      );
    });

  } catch (error) {
    console.error(`✗ Error publishing command ${command.id}:`, error.message);
  }
}

console.log('MQTT Publisher Service starting...');
```

### Service 2: MQTT Subscriber (Node.js)

Tạo file `mqtt-subscriber-service.js`:

```javascript
const mqtt = require('mqtt');
const axios = require('axios');

// Directus configuration
const DIRECTUS_URL = process.env.DIRECTUS_URL || 'http://localhost:8055';
const DIRECTUS_TOKEN = process.env.DIRECTUS_TOKEN;

// MQTT configuration
const MQTT_BROKER = process.env.MQTT_BROKER || 'broker.hivemq.com';
const MQTT_PORT = parseInt(process.env.MQTT_PORT || '1883');
const MQTT_USERNAME = process.env.MQTT_USERNAME || '';
const MQTT_PASSWORD = process.env.MQTT_PASSWORD || '';

// Connect to MQTT broker
const mqttClient = mqtt.connect(`mqtt://${MQTT_BROKER}:${MQTT_PORT}`, {
  username: MQTT_USERNAME,
  password: MQTT_PASSWORD,
  clientId: 'directus-subscriber-' + Math.random().toString(16).substr(2, 8)
});

mqttClient.on('connect', () => {
  console.log('✓ Connected to MQTT broker:', MQTT_BROKER);

  // Subscribe to all device status topics
  mqttClient.subscribe('device/+/status', { qos: 1 }, (err) => {
    if (err) {
      console.error('✗ Failed to subscribe:', err);
    } else {
      console.log('✓ Subscribed to: device/+/status');
    }
  });
});

mqttClient.on('message', async (topic, payload) => {
  try {
    console.log(`← Message received on ${topic}`);

    const message = JSON.parse(payload.toString());
    const commandId = message.command_id;
    const status = message.status;
    const result = message.result || null;
    const errorMessage = message.error_message || null;

    if (!commandId) {
      console.log('  Skipping (no command_id)');
      return;
    }

    // Update command in Directus
    await axios.patch(
      `${DIRECTUS_URL}/items/device_commands/${commandId}`,
      {
        status: status,
        result: result,
        error_message: errorMessage,
        executed_at: new Date().toISOString()
      },
      {
        headers: { 'Authorization': `Bearer ${DIRECTUS_TOKEN}` }
      }
    );

    console.log(`✓ Updated command ${commandId}: ${status}`);

  } catch (error) {
    console.error('✗ Error processing message:', error.message);
  }
});

mqttClient.on('error', (err) => {
  console.error('✗ MQTT error:', err);
});

console.log('MQTT Subscriber Service starting...');
```

### Install Dependencies

```bash
npm init -y
npm install mqtt axios dotenv
```

### Environment Variables

Tạo file `.env`:

```bash
DIRECTUS_URL=http://localhost:8055
DIRECTUS_TOKEN=your-static-token-here
MQTT_BROKER=broker.hivemq.com
MQTT_PORT=1883
MQTT_USERNAME=
MQTT_PASSWORD=
```

### Run Services

```bash
# Terminal 1: Publisher
node mqtt-publisher-service.js

# Terminal 2: Subscriber
node mqtt-subscriber-service.js
```

Hoặc dùng PM2:

```bash
npm install -g pm2

pm2 start mqtt-publisher-service.js --name mqtt-publisher
pm2 start mqtt-subscriber-service.js --name mqtt-subscriber
pm2 save
pm2 startup
```

## 4. Testing

### Test với MQTTX Client

1. Download MQTTX: https://mqttx.app
2. Connect to broker
3. Subscribe to topic: `device/+/status`
4. Publish test command:

Topic: `device/AA:BB:CC:DD:EE:FF/commands`

Payload:
```json
{
  "command_id": "test-12345",
  "type": "get_info",
  "params": {},
  "timestamp": 1234567890
}
```

### Test với mosquitto_pub/sub

```bash
# Subscribe to status updates
mosquitto_sub -h broker.hivemq.com -t "device/+/status"

# Publish test command
mosquitto_pub -h broker.hivemq.com \
  -t "device/AA:BB:CC:DD:EE:FF/commands" \
  -m '{"command_id":"test-123","type":"get_info","params":{},"timestamp":1234567890}'
```

## 5. Usage Flow

1. **Create Command in Directus:**
   - Vào collection `device_commands`
   - Create new item:
     - Device: Chọn device
     - Command Type: `enroll`
     - Fingerprint ID: `5`
     - Employee Code: `EMP001`
   - Save (status = `pending`)

2. **MQTT Publisher sẽ:**
   - Poll và tìm thấy command pending
   - Publish to MQTT topic `device/{mac}/commands`
   - Update status = `processing`

3. **ESP32 Device sẽ:**
   - Receive command qua MQTT
   - Execute enrollment
   - Publish result to `device/{mac}/status`

4. **MQTT Subscriber sẽ:**
   - Receive status message
   - Update Directus command record
   - Status = `completed` hoặc `failed`

## 6. MQTT Topics

### Command Topic (ESP32 subscribes)
- Pattern: `device/{device_mac}/commands`
- QoS: 1 (at least once)
- Payload:
```json
{
  "command_id": "uuid",
  "type": "enroll|delete|delete_all|get_info|restart",
  "params": {
    "fingerprint_id": 5,
    "employee_code": "EMP001"
  },
  "timestamp": 1234567890
}
```

### Status Topic (ESP32 publishes)
- Pattern: `device/{device_mac}/status`
- QoS: 0 (best effort)
- Payload:
```json
{
  "command_id": "uuid",
  "status": "completed|failed|processing|online|offline",
  "result": {
    "fingerprint_id": 5,
    "confidence": 142,
    "template_size": 512
  },
  "error_message": null,
  "timestamp": 1234567890
}
```

## 7. Security Considerations

### MQTT Authentication
- Use username/password auth
- Consider per-device credentials
- Enable TLS (port 8883) for production

### Directus API
- Use static token with appropriate permissions
- Limit token scope to device_commands collection only

### MQTT ACL (Access Control List)
Configure broker ACL để device chỉ có thể:
- Subscribe: `device/{its_mac}/commands`
- Publish: `device/{its_mac}/status`

Example ACL (Mosquitto):
```
user esp32_device
topic read device/%u/commands
topic write device/%u/status
```

## 8. Troubleshooting

### ESP32 không kết nối được MQTT
- Check WiFi connection
- Verify broker URL và port
- Check username/password
- Try with public broker first: `broker.hivemq.com:1883`

### Commands không được publish
- Check MQTT Publisher service logs
- Verify Directus token permissions
- Check command status in DB

### Status updates không về Directus
- Check MQTT Subscriber service logs
- Verify topic subscription: `device/+/status`
- Check payload format

### Connection drops
- Check MQTT keepalive setting (60s recommended)
- Enable auto-reconnect with exponential backoff
- Monitor broker connection limits

## Next Steps

1. Implement Directus Flows để auto-trigger publisher
2. Add Web UI for real-time command monitoring
3. Implement rate limiting
4. Add command history và audit log
5. Set up monitoring và alerting
