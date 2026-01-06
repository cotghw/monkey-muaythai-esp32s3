# Directus WebSocket Setup Guide

Setup required Directus collections and fields for remote device control via WebSocket.

## Step 1: Create device_commands Collection

### Using Directus Data Studio UI:

1. Navigate to **Settings → Data Model**
2. Click **Create Collection**
3. Collection Name: `device_commands`
4. Primary Key: Auto-increment UUID
5. Click **Save**

### Add Fields:

#### 1. device_id (Many-to-One to devices)
- **Field Type:** UUID
- **Interface:** Select Dropdown (M2O)
- **Relation:** Many device_commands → One device
- **Display Template:** `{{device_name}} ({{device_mac}})`
- **Required:** Yes
- **Width:** Half

#### 2. command_type (Enum)
- **Field Type:** String
- **Interface:** Select Dropdown
- **Choices:**
  - `enroll` - Enroll new fingerprint
  - `delete` - Delete specific fingerprint
  - `delete_all` - Delete all fingerprints
  - `get_info` - Get device/sensor info
  - `restart` - Restart device
- **Required:** Yes
- **Default:** `enroll`
- **Width:** Half

#### 3. fingerprint_id (Integer)
- **Field Type:** Integer
- **Interface:** Input
- **Min:** 1
- **Max:** 127
- **Required:** No (only for delete command)
- **Note:** "Fingerprint ID (1-127) for delete command"
- **Width:** Half
- **Condition:** Hidden unless command_type = "delete"

#### 4. employee_code (String)
- **Field Type:** String
- **Interface:** Input
- **Max Length:** 50
- **Required:** No (only for enroll command)
- **Note:** "Employee code for fingerprint enrollment"
- **Width:** Half
- **Condition:** Hidden unless command_type = "enroll"

#### 5. status (Enum)
- **Field Type:** String
- **Interface:** Select Dropdown
- **Display:** Labels
- **Choices:**
  - `pending` - Waiting for device (Yellow)
  - `processing` - Device executing (Blue)
  - `completed` - Success (Green)
  - `failed` - Error (Red)
- **Default:** `pending`
- **Required:** Yes
- **Width:** Half
- **Read-only:** Yes (updated by device)

#### 6. result (JSON)
- **Field Type:** JSON
- **Interface:** Code (JSON)
- **Required:** No
- **Note:** "Command execution result from device"
- **Width:** Full
- **Read-only:** Yes

#### 7. error_message (Text)
- **Field Type:** Text
- **Interface:** Textarea
- **Required:** No
- **Width:** Full
- **Read-only:** Yes
- **Condition:** Hidden unless status = "failed"

#### 8. created_at (Timestamp)
- **Field Type:** Timestamp
- **Special:** Date Created
- **Interface:** Datetime
- **Display:** Relative time
- **Width:** Half
- **Read-only:** Yes

#### 9. executed_at (Timestamp)
- **Field Type:** Timestamp
- **Interface:** Datetime
- **Required:** No
- **Width:** Half
- **Read-only:** Yes

#### 10. created_by (Many-to-One to directus_users)
- **Field Type:** UUID
- **Special:** User Created
- **Interface:** Select Dropdown (M2O)
- **Display Template:** `{{first_name}} {{last_name}}`
- **Width:** Half
- **Read-only:** Yes

### Collection Settings:
- **Icon:** `play_arrow`
- **Color:** `#2196F3`
- **Note:** "Remote commands for fingerprint devices"
- **Display Template:** `{{command_type}} on {{device_id.device_name}}`
- **Archive Field:** `status`
- **Archive Value:** `completed`
- **Unarchive Value:** `pending`

## Step 2: Update devices Collection

Add new fields to track device online status:

#### last_seen_at (Timestamp)
- **Field Type:** Timestamp
- **Interface:** Datetime
- **Display:** Relative time
- **Required:** No
- **Note:** "Last WebSocket heartbeat from device"
- **Width:** Half

#### websocket_connected (Boolean)
- **Field Type:** Boolean
- **Interface:** Toggle
- **Default:** `false`
- **Required:** Yes
- **Display:** Badge
- **Display Options:**
  - True: "Online" (Green)
  - False: "Offline" (Red)
- **Width:** Half

## Step 3: Create Indexes (Performance)

Run SQL in Directus Console:

```sql
-- Index for querying pending commands by device
CREATE INDEX idx_device_commands_device_status
ON device_commands(device_id, status);

-- Index for cleanup/audit queries
CREATE INDEX idx_device_commands_created
ON device_commands(created_at DESC);

-- Index for device online status queries
CREATE INDEX idx_devices_last_seen
ON devices(last_seen_at DESC);
```

## Step 4: Set Permissions

### Public Role (Optional - for testing):
- `device_commands`: Read only
- `devices`: Read only (websocket_connected, last_seen_at)

### Admin Role:
- `device_commands`: Full CRUD
- `devices`: Full CRUD

### Device Role (for ESP32):
- `device_commands`:
  - Read: Filter by `device_id._eq=$CURRENT_USER.device_id`
  - Update: Only `status`, `result`, `error_message`, `executed_at`
- `devices`:
  - Update: Only `last_seen_at`, `websocket_connected`

## Step 5: Test Data

Create test command via Data Studio:

```json
{
  "device_id": "93e54e6b-4dde-4022-906b-abc81553020e",
  "command_type": "get_info",
  "status": "pending"
}
```

Expected ESP32 behavior:
1. Receive via WebSocket within 100ms
2. Update status → "processing"
3. Execute command
4. Update status → "completed" with result

## WebSocket Subscription (ESP32)

ESP32 will subscribe with this filter:

```json
{
  "type": "subscribe",
  "collection": "device_commands",
  "query": {
    "filter": {
      "device_id": {
        "_eq": "93e54e6b-4dde-4022-906b-abc81553020e"
      },
      "status": {
        "_eq": "pending"
      }
    },
    "fields": ["*"]
  }
}
```

## Monitoring

Track command execution in Directus:

1. **Pending Commands:** `status = pending` + `created_at < 1 minute ago`
2. **Failed Commands:** `status = failed` + `error_message not null`
3. **Device Response Time:** `executed_at - created_at`

## Cleanup Policy (Optional)

Auto-archive completed commands after 30 days using Directus Flow:

**Trigger:** Scheduled (Daily at 2 AM)

**Operation:** Update Items
- Collection: `device_commands`
- Filter: `status = completed AND created_at < $NOW(-30 days)`
- Data: `{"archived": true}`

## Troubleshooting

### Command not received by ESP32:
1. Check `websocket_connected` = true in devices table
2. Verify device_id matches
3. Check Directus logs for WebSocket errors
4. Test with simple curl: `curl -X POST {directus}/items/device_commands -d {...}`

### Command stuck in "processing":
1. Check ESP32 serial logs for errors
2. Verify fingerprint sensor is responding
3. Set timeout on ESP32 (30s) to auto-fail stuck commands

### Multiple devices receive same command:
1. Verify WebSocket subscription filter includes device_id
2. Check Directus permissions on device_commands collection
