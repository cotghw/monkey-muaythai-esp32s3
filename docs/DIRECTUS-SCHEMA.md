# Directus Schema - Fingerprint Attendance System

Schema cho Directus CMS để quản lý hệ thống chấm công vân tay.

## Collections Overview

```
employees          → Thông tin nhân viên
fingerprints       → Dữ liệu vân tay
devices            → ESP32 devices
attendance_logs    → Log chấm công
```

---

## 1. Collection: `employees`

Quản lý thông tin nhân viên.

### Fields

| Field Name | Type | Interface | Required | Notes |
|------------|------|-----------|----------|-------|
| `id` | UUID | Primary Key | Yes | Auto-generated |
| `employee_code` | String | Input | Yes | Mã nhân viên (unique) |
| `full_name` | String | Input | Yes | Họ tên đầy đủ |
| `email` | String | Input | No | Email |
| `phone` | String | Input | No | Số điện thoại |
| `department` | String | Dropdown | No | Phòng ban (IT, HR, Sales...) |
| `position` | String | Input | No | Chức vụ |
| `status` | String | Dropdown | Yes | active, inactive, suspended |
| `date_hired` | Date | Date | No | Ngày vào làm |
| `avatar` | File | Image | No | Ảnh đại diện |
| `created_at` | Timestamp | Datetime | Yes | Auto |
| `updated_at` | Timestamp | Datetime | Yes | Auto |

### Validation Rules

- `employee_code`: Unique, uppercase, alphanumeric
- `email`: Valid email format, unique
- `status`: Default = "active"

### Sample Data

```json
{
  "id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "employee_code": "EMP001",
  "full_name": "Nguyễn Văn A",
  "email": "nguyenvana@company.com",
  "phone": "0901234567",
  "department": "IT",
  "position": "Software Engineer",
  "status": "active",
  "date_hired": "2024-01-15"
}
```

---

## 2. Collection: `fingerprints`

Lưu trữ fingerprint template data.

### Fields

| Field Name | Type | Interface | Required | Notes |
|------------|------|-----------|----------|-------|
| `id` | UUID | Primary Key | Yes | Auto-generated |
| `employee_id` | M2O | Many-to-One | Yes | Relation → employees |
| `device_id` | M2O | Many-to-One | Yes | Relation → devices |
| `fingerprint_id` | Integer | Input | Yes | ID trong R307 sensor (1-127) |
| `template_data` | Text | Textarea | Yes | Base64 encoded template (512 bytes) |
| `confidence_threshold` | Integer | Input | No | Min confidence để accept (default: 80) |
| `last_verified_at` | Timestamp | Datetime | No | Lần verify cuối cùng |
| `last_confidence` | Integer | Input | No | Confidence score cuối cùng |
| `is_active` | Boolean | Toggle | Yes | Enable/disable fingerprint này |
| `enrolled_at` | Timestamp | Datetime | Yes | Ngày đăng ký |
| `created_at` | Timestamp | Datetime | Yes | Auto |
| `updated_at` | Timestamp | Datetime | Yes | Auto |

### Validation Rules

- `fingerprint_id`: 1-127
- `confidence_threshold`: 0-200 (default: 80)
- `is_active`: Default = true

### Unique Constraint

- Combination (`device_id`, `fingerprint_id`) must be unique

### Sample Data

```json
{
  "id": "f1f2f3f4-f5f6-f7f8-f9f0-f1f2f3f4f5f6",
  "employee_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "device_id": "d1d2d3d4-d5d6-d7d8-d9d0-d1d2d3d4d5d6",
  "fingerprint_id": 1,
  "template_data": "AgECAwQFBgcICQoLDA0ODxAREhMUFRYXGBka...",
  "confidence_threshold": 80,
  "is_active": true,
  "enrolled_at": "2024-01-15T08:30:00Z"
}
```

---

## 3. Collection: `devices`

Quản lý ESP32 devices.

### Fields

| Field Name | Type | Interface | Required | Notes |
|------------|------|-----------|----------|-------|
| `id` | UUID | Primary Key | Yes | Auto-generated |
| `device_mac` | String | Input | Yes | MAC address (unique) |
| `device_name` | String | Input | Yes | Tên thiết bị |
| `location` | String | Input | No | Vị trí đặt (Văn phòng A, Cổng B...) |
| `status` | String | Dropdown | Yes | online, offline, maintenance |
| `last_seen_at` | Timestamp | Datetime | No | Lần kết nối cuối |
| `firmware_version` | String | Input | No | Phiên bản firmware |
| `ip_address` | String | Input | No | IP address hiện tại |
| `total_scans` | Integer | Input | No | Tổng số lần scan |
| `is_active` | Boolean | Toggle | Yes | Enable/disable device |
| `notes` | Text | Textarea | No | Ghi chú |
| `created_at` | Timestamp | Datetime | Yes | Auto |
| `updated_at` | Timestamp | Datetime | Yes | Auto |

### Validation Rules

- `device_mac`: Unique, format AA:BB:CC:DD:EE:FF
- `status`: Default = "offline"
- `is_active`: Default = true

### Sample Data

```json
{
  "id": "d1d2d3d4-d5d6-d7d8-d9d0-d1d2d3d4d5d6",
  "device_mac": "AA:BB:CC:DD:EE:FF",
  "device_name": "ESP32-Gateway-01",
  "location": "Văn phòng tầng 2",
  "status": "online",
  "last_seen_at": "2024-01-15T10:30:00Z",
  "ip_address": "192.168.1.100",
  "is_active": true
}
```

---

## 4. Collection: `attendance_logs`

Log mỗi lần chấm công.

### Fields

| Field Name | Type | Interface | Required | Notes |
|------------|------|-----------|----------|-------|
| `id` | UUID | Primary Key | Yes | Auto-generated |
| `employee_id` | M2O | Many-to-One | No | Relation → employees (null nếu không nhận diện được) |
| `device_id` | M2O | Many-to-One | Yes | Relation → devices |
| `fingerprint_id` | Integer | Input | No | ID vân tay (1-127) |
| `confidence` | Integer | Input | No | Confidence score từ sensor |
| `access_granted` | Boolean | Toggle | Yes | true = cho phép, false = từ chối |
| `reason` | String | Dropdown | No | success, low_confidence, not_registered, inactive |
| `timestamp` | Timestamp | Datetime | Yes | Thời gian chấm |
| `check_type` | String | Dropdown | No | check_in, check_out, unknown |
| `template_data` | Text | Textarea | No | Base64 template (optional, để audit) |
| `created_at` | Timestamp | Datetime | Yes | Auto |

### Validation Rules

- `access_granted`: Required
- `timestamp`: Default = now
- `reason`: Default = "success" if granted

### Indexes

- Index on `employee_id`
- Index on `device_id`
- Index on `timestamp` (for reporting)

### Sample Data

```json
{
  "id": "l1l2l3l4-l5l6-l7l8-l9l0-l1l2l3l4l5l6",
  "employee_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "device_id": "d1d2d3d4-d5d6-d7d8-d9d0-d1d2d3d4d5d6",
  "fingerprint_id": 1,
  "confidence": 142,
  "access_granted": true,
  "reason": "success",
  "timestamp": "2024-01-15T08:30:45Z",
  "check_type": "check_in"
}
```

---

## Relationships Diagram

```
employees (1) ──────< (M) fingerprints
devices (1) ────────< (M) fingerprints
employees (1) ──────< (M) attendance_logs
devices (1) ────────< (M) attendance_logs
```

---

## Directus Flow (Optional - Advanced)

Tạo Flow để tự động xử lý business logic:

### Flow 1: Auto-disable inactive employees

```yaml
Trigger: Schedule (Daily at 00:00)
Operation:
  - Filter employees where status = "inactive"
  - Update related fingerprints: is_active = false
```

### Flow 2: Send notification on failed access

```yaml
Trigger: attendance_logs created
Condition: access_granted = false
Operation:
  - Send email to security@company.com
  - Log to Slack/Discord webhook
```

### Flow 3: Update device last_seen

```yaml
Trigger: attendance_logs created
Operation:
  - Update devices.last_seen_at = now
  - Update devices.status = "online"
  - Increment devices.total_scans
```

---

## Directus Roles & Permissions

### Role: `esp32_device` (for API access)

**employees:**
- Read: All (to get employee info)

**fingerprints:**
- Read: All (to verify fingerprints)
- Create: All (to enroll new fingerprints)

**devices:**
- Read: Filter by device_mac (only own device)
- Update: Filter by device_mac (update last_seen, status)

**attendance_logs:**
- Create: All (to log attendance)

### Role: `admin` (for web interface)

- Full access to all collections

### Role: `hr_manager`

**employees:** Read, Update
**fingerprints:** Read, Delete
**attendance_logs:** Read only
**devices:** Read only

---

## Access Token Setup

1. Tạo **Static Token** trong Directus:
   - Settings → Access Tokens → Create New
   - Role: `esp32_device`
   - Copy token

2. Cấu hình trong ESP32:
   ```cpp
   // src/config.h
   #define CMS_API_URL "https://your-directus.com"
   #define CMS_API_TOKEN "your-static-token-here"
   ```

---

## API Endpoints

Directus tự động tạo REST API:

### Get all employees
```http
GET /items/employees
Authorization: Bearer YOUR_TOKEN
```

### Get fingerprints for device
```http
GET /items/fingerprints?filter[device_id][_eq]=DEVICE_UUID
Authorization: Bearer YOUR_TOKEN
```

### Create attendance log
```http
POST /items/attendance_logs
Authorization: Bearer YOUR_TOKEN
Content-Type: application/json

{
  "employee_id": "UUID",
  "device_id": "UUID",
  "fingerprint_id": 1,
  "confidence": 142,
  "access_granted": true,
  "reason": "success",
  "timestamp": "2024-01-15T08:30:00Z"
}
```

---

## Quick Setup Guide

### 1. Create Collections

Trong Directus Admin:
- Settings → Data Model → Create Collection
- Tạo 4 collections theo schema trên

### 2. Set up Relationships

- `fingerprints.employee_id` → M2O → `employees.id`
- `fingerprints.device_id` → M2O → `devices.id`
- `attendance_logs.employee_id` → M2O → `employees.id`
- `attendance_logs.device_id` → M2O → `devices.id`

### 3. Create API Role

- Settings → Roles & Permissions → Create `esp32_device`
- Set permissions theo bảng trên

### 4. Generate Token

- Settings → Access Tokens → Create
- Role: `esp32_device`
- Token: Copy vào config.h

### 5. Test API

```bash
curl https://your-directus.com/items/employees \
  -H "Authorization: Bearer YOUR_TOKEN"
```

---

## Sample Data Import

Tạo file `sample-data.json`:

```json
{
  "employees": [
    {
      "employee_code": "EMP001",
      "full_name": "Nguyễn Văn A",
      "email": "nguyenvana@company.com",
      "department": "IT",
      "status": "active"
    }
  ],
  "devices": [
    {
      "device_mac": "AA:BB:CC:DD:EE:FF",
      "device_name": "ESP32-Gateway-01",
      "location": "Văn phòng tầng 2",
      "status": "offline",
      "is_active": true
    }
  ]
}
```

Import qua Directus UI hoặc API.

---

## Notes

- **Template data** được lưu dạng TEXT (Base64), kích thước ~700 chars
- **UUID** được Directus auto-generate, ESP32 chỉ cần gửi MAC address
- **Timestamps** nên dùng ISO 8601 format
- **Confidence threshold** có thể customize cho từng fingerprint
- **Cascade delete** nên enable để xóa fingerprints khi xóa employee
