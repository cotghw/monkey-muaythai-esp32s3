# CMS API Contract - Fingerprint Authentication System

Tài liệu này mô tả API contract giữa ESP32-S3 fingerprint device và CMS backend server.

## Overview

ESP32-S3 gửi **fingerprint template data** (512 bytes encoded Base64) đến CMS để:
1. **Verify**: Xác thực vân tay và kiểm tra access permission
2. **Enroll**: Đăng ký vân tay mới vào database

## Authentication

### API Key (Optional)

Nếu CMS yêu cầu authentication, ESP32 sẽ gửi API key qua header:

```http
Authorization: Bearer YOUR_API_KEY
```

Cấu hình trong `src/config.h`:

```cpp
#define CMS_API_KEY "your-api-key-here"
```

## API Endpoints

### 1. Verify Fingerprint

Xác thực vân tay và kiểm tra access permission.

---

**Endpoint:** `POST /api/fingerprint/verify`

**Headers:**
```http
Content-Type: application/json
Authorization: Bearer YOUR_API_KEY (optional)
```

**Request Body:**
```json
{
  "device_id": "AA:BB:CC:DD:EE:FF",
  "fingerprint_id": 1,
  "confidence": 142,
  "template_data": "base64_encoded_512_bytes_string...",
  "timestamp": 1234567890
}
```

**Request Fields:**

| Field | Type | Description | Example |
|-------|------|-------------|---------|
| `device_id` | string | MAC address của ESP32 device | `"AA:BB:CC:DD:EE:FF"` |
| `fingerprint_id` | integer | ID vân tay trong R307 sensor (1-127) | `1` |
| `confidence` | integer | Độ tin cậy từ sensor (50-200, càng cao càng chính xác) | `142` |
| `template_data` | string | Base64 encoded fingerprint template (512 bytes raw → ~683 chars base64) | `"iVBORw0KGgo..."` |
| `timestamp` | integer | Unix timestamp (milliseconds since boot) | `1234567890` |

**Response Success (200 OK):**
```json
{
  "access": true,
  "user": {
    "id": 123,
    "name": "Nguyen Van A",
    "role": "employee",
    "department": "IT"
  },
  "timestamp": "2024-01-15T08:30:00Z",
  "message": "Access granted"
}
```

**Response Denied (200 OK):**
```json
{
  "access": false,
  "reason": "Fingerprint not registered",
  "timestamp": "2024-01-15T08:30:00Z"
}
```

**Response Error (400 Bad Request):**
```json
{
  "error": "Invalid request",
  "message": "Missing required field: template_data"
}
```

**Response Error (401 Unauthorized):**
```json
{
  "error": "Unauthorized",
  "message": "Invalid API key"
}
```

**Response Error (500 Internal Server Error):**
```json
{
  "error": "Server error",
  "message": "Database connection failed"
}
```

---

### 2. Enroll Fingerprint

Đăng ký vân tay mới vào CMS database.

---

**Endpoint:** `POST /api/fingerprint/enroll`

**Headers:**
```http
Content-Type: application/json
Authorization: Bearer YOUR_API_KEY (optional)
```

**Request Body:**
```json
{
  "device_id": "AA:BB:CC:DD:EE:FF",
  "fingerprint_id": 1,
  "template_data": "base64_encoded_512_bytes_string...",
  "user_name": "Nguyen Van A",
  "user_email": "nguyenvana@company.com",
  "user_role": "employee",
  "timestamp": 1234567890
}
```

**Request Fields:**

| Field | Type | Required | Description | Example |
|-------|------|----------|-------------|---------|
| `device_id` | string | Yes | MAC address của ESP32 | `"AA:BB:CC:DD:EE:FF"` |
| `fingerprint_id` | integer | Yes | ID vân tay (1-127) | `1` |
| `template_data` | string | Yes | Base64 encoded template | `"iVBORw0KGgo..."` |
| `user_name` | string | No | Tên người dùng | `"Nguyen Van A"` |
| `user_email` | string | No | Email người dùng | `"email@company.com"` |
| `user_role` | string | No | Vai trò | `"employee"` |
| `timestamp` | integer | Yes | Unix timestamp | `1234567890` |

**Response Success (201 Created):**
```json
{
  "success": true,
  "message": "Fingerprint enrolled successfully",
  "user_id": 123,
  "fingerprint_id": 1
}
```

**Response Error (400 Bad Request):**
```json
{
  "success": false,
  "message": "Fingerprint already exists for this user"
}
```

**Response Error (409 Conflict):**
```json
{
  "success": false,
  "message": "Fingerprint ID #1 already registered to another user"
}
```

---

## Data Format Details

### Fingerprint Template Data

**Raw Format:**
- Size: **512 bytes**
- Type: Binary array (uint8_t[512])
- Content: Compressed feature points của vân tay (không phải ảnh gốc)

**Base64 Encoded:**
- Size: **~683 characters**
- Encoding: Standard Base64
- Example: `"AgECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCU..."`

**Decoding (Backend):**

```python
# Python
import base64

template_base64 = request.json['template_data']
template_bytes = base64.b64decode(template_base64)
# template_bytes is now 512 bytes binary data
```

```javascript
// Node.js
const templateBase64 = req.body.template_data;
const templateBuffer = Buffer.from(templateBase64, 'base64');
// templateBuffer is now 512 bytes
```

```php
// PHP
$templateBase64 = $request['template_data'];
$templateBytes = base64_decode($templateBase64);
// $templateBytes is now 512 bytes binary string
```

### Device ID

- Format: MAC address (colon-separated hex)
- Example: `"AA:BB:CC:DD:EE:FF"`
- Unique identifier cho mỗi ESP32 device
- Dùng để tracking logs từ device nào

### Confidence Score

- Range: **50-200**
- Type: Integer
- Meaning:
  - < 50: Không chấp nhận
  - 50-100: Độ tin cậy thấp
  - 100-150: Độ tin cậy vừa (acceptable)
  - 150+: Độ tin cậy cao (excellent)

CMS có thể reject nếu confidence < threshold (ví dụ 80).

## Database Schema Suggestion

### Table: `fingerprints`

```sql
CREATE TABLE fingerprints (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    device_id VARCHAR(17) NOT NULL,  -- MAC address
    fingerprint_id INT NOT NULL,     -- ID in R307 sensor (1-127)
    template_data BLOB NOT NULL,     -- 512 bytes binary
    confidence INT,                  -- Latest confidence score
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

    UNIQUE KEY unique_device_fingerprint (device_id, fingerprint_id),
    INDEX idx_user (user_id),
    INDEX idx_device (device_id)
);
```

### Table: `access_logs`

```sql
CREATE TABLE access_logs (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT,
    device_id VARCHAR(17) NOT NULL,
    fingerprint_id INT,
    confidence INT,
    access_granted BOOLEAN NOT NULL,
    reason VARCHAR(255),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    INDEX idx_user (user_id),
    INDEX idx_device (device_id),
    INDEX idx_timestamp (timestamp)
);
```

## Fingerprint Matching Algorithm

Backend CMS cần implement algorithm để so sánh fingerprint templates.

### Option 1: Exact Match (Simple)

```python
def verify_fingerprint(template_from_device, template_in_db):
    return template_from_device == template_in_db
```

**Pros:** Đơn giản, nhanh
**Cons:** Không chính xác (vân tay có thể hơi khác mỗi lần scan)

### Option 2: Hamming Distance (Recommended)

```python
def hamming_distance(template1, template2):
    """Calculate number of different bits"""
    distance = 0
    for b1, b2 in zip(template1, template2):
        xor = b1 ^ b2
        distance += bin(xor).count('1')
    return distance

def verify_fingerprint(template_device, template_db, threshold=50):
    """
    threshold: Maximum allowed bit differences
    Lower threshold = stricter matching
    """
    distance = hamming_distance(template_device, template_db)
    return distance <= threshold
```

**Pros:** Chính xác hơn, cho phép tolerant với noise
**Cons:** Phức tạp hơn, tốn CPU

### Option 3: Use Existing Library

**Python:** `pyfingerprint` library
**Node.js:** Custom implementation hoặc native addon
**PHP:** GD library + custom algorithm

## Security Recommendations

### 1. Use HTTPS

**CRITICAL:** Production MUST use HTTPS để encrypt fingerprint data.

```cpp
// src/config.h
#define CMS_API_URL "https://your-cms.com/api/fingerprint/verify"
```

ESP32 cần configure SSL certificate:

```cpp
#include <WiFiClientSecure.h>

WiFiClientSecure client;
client.setCACert(root_ca);  // Set root CA certificate
```

### 2. API Signature

Thêm HMAC signature để verify request integrity:

```cpp
// Pseudo-code
String payload = createJSONPayload();
String signature = hmac_sha256(payload, SECRET_KEY);
http.addHeader("X-Signature", signature);
```

Backend verify:

```python
import hmac
import hashlib

def verify_signature(payload, signature, secret_key):
    expected = hmac.new(
        secret_key.encode(),
        payload.encode(),
        hashlib.sha256
    ).hexdigest()
    return hmac.compare_digest(expected, signature)
```

### 3. Rate Limiting

Implement rate limiting để prevent brute-force attacks:

```
Maximum 10 requests/minute per device_id
Maximum 100 requests/hour per device_id
```

### 4. Timestamp Validation

Reject requests với timestamp quá cũ (> 5 phút):

```python
import time

current_time = int(time.time() * 1000)
request_time = request.json['timestamp']

if abs(current_time - request_time) > 300000:  # 5 minutes
    return {"error": "Request expired"}, 400
```

## Testing

### cURL Examples

**Verify Fingerprint:**

```bash
curl -X POST https://your-cms.com/api/fingerprint/verify \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_API_KEY" \
  -d '{
    "device_id": "AA:BB:CC:DD:EE:FF",
    "fingerprint_id": 1,
    "confidence": 142,
    "template_data": "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==",
    "timestamp": 1234567890
  }'
```

**Enroll Fingerprint:**

```bash
curl -X POST https://your-cms.com/api/fingerprint/enroll \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_API_KEY" \
  -d '{
    "device_id": "AA:BB:CC:DD:EE:FF",
    "fingerprint_id": 1,
    "template_data": "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==",
    "user_name": "Nguyen Van A",
    "timestamp": 1234567890
  }'
```

### Postman Collection

Import vào Postman để test:

```json
{
  "info": {
    "name": "Fingerprint API",
    "schema": "https://schema.getpostman.com/json/collection/v2.1.0/collection.json"
  },
  "item": [
    {
      "name": "Verify Fingerprint",
      "request": {
        "method": "POST",
        "header": [
          {
            "key": "Content-Type",
            "value": "application/json"
          },
          {
            "key": "Authorization",
            "value": "Bearer {{API_KEY}}"
          }
        ],
        "body": {
          "mode": "raw",
          "raw": "{\n  \"device_id\": \"AA:BB:CC:DD:EE:FF\",\n  \"fingerprint_id\": 1,\n  \"confidence\": 142,\n  \"template_data\": \"{{TEMPLATE_DATA}}\",\n  \"timestamp\": {{TIMESTAMP}}\n}"
        },
        "url": {
          "raw": "{{BASE_URL}}/api/fingerprint/verify",
          "host": ["{{BASE_URL}}"],
          "path": ["api", "fingerprint", "verify"]
        }
      }
    }
  ]
}
```

## Performance Considerations

### Response Time

- Target: < 500ms
- Maximum acceptable: 2 seconds
- ESP32 HTTP timeout: 10 seconds

### Database Optimization

1. **Index** trên `device_id` và `fingerprint_id`
2. **Cache** fingerprint templates trong Redis
3. **Partition** access_logs table by date

### Scalability

- Single server: ~100 requests/second
- Load balanced: ~1000 requests/second
- Consider using:
  - Redis cache
  - Message queue (RabbitMQ/Kafka)
  - Microservices architecture

## Error Handling

### ESP32 Side

Nếu CMS không response hoặc error:

1. Retry 3 lần với exponential backoff
2. Log error vào SPIFFS/SD card
3. Hiển thị error trên Serial Monitor
4. LED feedback (Red LED)

### CMS Side

Log tất cả errors với context:

```json
{
  "timestamp": "2024-01-15T08:30:00Z",
  "level": "ERROR",
  "device_id": "AA:BB:CC:DD:EE:FF",
  "endpoint": "/api/fingerprint/verify",
  "error": "Database connection timeout",
  "stack_trace": "..."
}
```

## Monitoring & Alerts

### Metrics to Track

1. **Request rate** (requests/minute)
2. **Success rate** (%)
3. **Average response time** (ms)
4. **Error rate** by type
5. **Active devices** (unique device_ids)

### Alerts

- Response time > 2s
- Error rate > 5%
- Database connection failures
- Suspicious activity (too many failed attempts)

## Contact & Support

Nếu có câu hỏi hoặc cần hỗ trợ integrate CMS backend, vui lòng liên hệ development team.
