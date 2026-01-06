# Directus Quick Start Guide

Hướng dẫn nhanh để setup Directus cho hệ thống chấm công vân tay.

## 📋 Prerequisites

- Directus instance đang chạy (cloud hoặc self-hosted)
- Admin access vào Directus
- Đã đọc [DIRECTUS-SCHEMA.md](DIRECTUS-SCHEMA.md)

---

## 🚀 Setup trong 5 phút

### Bước 1: Tạo Collections

Trong Directus Admin Panel:

**Settings → Data Model → Create Collection**

Tạo 4 collections:

1. **employees**
2. **fingerprints**
3. **devices**
4. **attendance_logs**

### Bước 2: Thêm Fields

#### Collection: `employees`

```
id             → UUID (Primary Key, Auto-generated)
employee_code  → String (Unique, Required)
full_name      → String (Required)
email          → String (Unique)
phone          → String
department     → String (Dropdown: IT, HR, Sales, Marketing)
position       → String
status         → String (Dropdown: active, inactive, suspended) Default: active
date_hired     → Date
avatar         → Image (File relation)
created_at     → Timestamp (Auto)
updated_at     → Timestamp (Auto)
```

#### Collection: `fingerprints`

```
id                   → UUID (Primary Key)
employee_id          → M2O → employees (Required)
device_id            → M2O → devices (Required)
fingerprint_id       → Integer (1-127, Required)
template_data        → Text (Required) - Base64 encoded
confidence_threshold → Integer (Default: 80)
last_verified_at     → Timestamp
last_confidence      → Integer
is_active            → Boolean (Default: true)
enrolled_at          → Timestamp (Auto)
created_at           → Timestamp (Auto)
updated_at           → Timestamp (Auto)
```

**Unique Constraint:** Create unique index on (`device_id`, `fingerprint_id`)

#### Collection: `devices`

```
id               → UUID (Primary Key)
device_mac       → String (Unique, Required) Format: AA:BB:CC:DD:EE:FF
device_name      → String (Required)
location         → String
status           → String (Dropdown: online, offline, maintenance) Default: offline
last_seen_at     → Timestamp
firmware_version → String
ip_address       → String
total_scans      → Integer (Default: 0)
is_active        → Boolean (Default: true)
notes            → Text
created_at       → Timestamp (Auto)
updated_at       → Timestamp (Auto)
```

#### Collection: `attendance_logs`

```
id             → UUID (Primary Key)
employee_id    → M2O → employees (Nullable)
device_id      → M2O → devices (Required)
fingerprint_id → Integer
confidence     → Integer
access_granted → Boolean (Required)
reason         → String (Dropdown: success, low_confidence, not_registered, inactive)
timestamp      → Timestamp (Required, Default: now)
check_type     → String (Dropdown: check_in, check_out, unknown)
template_data  → Text (Optional, for audit)
created_at     → Timestamp (Auto)
```

### Bước 3: Setup Relationships

Trong Data Model, setup relationships:

1. `fingerprints.employee_id` → Many-to-One → `employees.id`
2. `fingerprints.device_id` → Many-to-One → `devices.id`
3. `attendance_logs.employee_id` → Many-to-One → `employees.id`
4. `attendance_logs.device_id` → Many-to-One → `devices.id`

### Bước 4: Tạo API Role

**Settings → Roles & Permissions → Create Role**

**Role Name:** `esp32_device`

**Permissions:**

| Collection | Create | Read | Update | Delete |
|------------|--------|------|--------|--------|
| employees | ❌ | ✅ All | ❌ | ❌ |
| fingerprints | ✅ All | ✅ All | ❌ | ❌ |
| devices | ✅ All | ✅ All | ✅ All | ❌ |
| attendance_logs | ✅ All | ✅ All | ❌ | ❌ |

**Field Permissions:** Allow all fields

### Bước 5: Generate Access Token

**Settings → Access Tokens → Create New**

- **Name:** ESP32 Fingerprint Device
- **Role:** `esp32_device`
- **Policy:** `Static Token`
- **Expires:** Never (hoặc set expiry date)

**Copy token** → Sẽ dùng trong ESP32 config

### Bước 6: Add Sample Data

#### Tạo Employee mẫu

```json
{
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

Content → employees → Create Item → Paste JSON

#### Tạo Device mẫu (Optional - ESP32 sẽ tự đăng ký)

```json
{
  "device_mac": "AA:BB:CC:DD:EE:FF",
  "device_name": "ESP32-Gateway-01",
  "location": "Văn phòng tầng 2",
  "status": "offline",
  "is_active": true
}
```

---

## 🔧 Cấu hình ESP32

### 1. Sửa config.h

```cpp
// src/config.h
#define WIFI_SSID "YourWiFi"
#define WIFI_PASSWORD "password"

#define DIRECTUS_URL "https://your-directus.com"  // NO trailing slash!
#define DIRECTUS_TOKEN "paste-your-token-here"
```

### 2. Build & Upload

```bash
pio lib install
pio run --target upload
pio device monitor
```

### 3. Test kết nối

Trong Serial Monitor, bạn sẽ thấy:

```
✓ KẾT NỐI WiFi THÀNH CÔNG!
✓ Device đã được đăng ký trong Directus
✓ Hệ thống sẵn sàng!
```

---

## 📝 Workflow sử dụng

### 1. Đăng ký vân tay

```
Serial Monitor:
> e
Nhập ID vân tay (1-127): 1
[Chấm vân tay 2 lần]
✓ Đã lưu vân tay ID #1 thành công!

Gửi vân tay lên CMS? (y/n): y
Nhập mã nhân viên (VD: EMP001): EMP001
✓ Đăng ký vân tay lên Directus thành công!
```

**Trong Directus:**
- Collection `fingerprints` → New item created
- Linked to employee EMP001
- Template data saved as Base64

### 2. Test login

```
Serial Monitor:
> t
[Chấm vân tay]
✓ Tìm thấy 1 fingerprints đã đăng ký
✓ Tìm thấy fingerprint khớp!
✓ ACCESS GRANTED!
✓ Đã log attendance
```

**Trong Directus:**
- Collection `attendance_logs` → New log created
- `access_granted: true`
- `reason: success`
- Timestamp recorded

### 3. Auto-login mode

```
Serial Monitor:
> a
🔵 AUTO-LOGIN MODE: ĐÃ BẬT

[Hệ thống tự động scan mỗi giây]
→ Phát hiện vân tay!
✓ ACCESS GRANTED!
[LED xanh sáng]
```

---

## 🔍 Testing API

### Test với cURL

#### Get employees

```bash
curl https://your-directus.com/items/employees \
  -H "Authorization: Bearer YOUR_TOKEN"
```

Response:
```json
{
  "data": [
    {
      "id": "uuid-here",
      "employee_code": "EMP001",
      "full_name": "Nguyễn Văn A",
      ...
    }
  ]
}
```

#### Get fingerprints for device

```bash
curl "https://your-directus.com/items/fingerprints?filter[device_id][_eq]=DEVICE_UUID" \
  -H "Authorization: Bearer YOUR_TOKEN"
```

#### Get attendance logs

```bash
curl "https://your-directus.com/items/attendance_logs?sort=-timestamp&limit=10" \
  -H "Authorization: Bearer YOUR_TOKEN"
```

---

## 📊 Directus Dashboard (Optional)

### Tạo Insight Dashboard

**Settings → Insights → Create Dashboard**

**Panels:**

1. **Total Employees**
   - Type: Metric
   - Collection: employees
   - Function: count

2. **Attendance Today**
   - Type: Time Series
   - Collection: attendance_logs
   - Filter: timestamp > today
   - Group by: hour

3. **Access Success Rate**
   - Type: Metric
   - Collection: attendance_logs
   - Function: percentage
   - Field: access_granted

4. **Top Employees (Most Check-ins)**
   - Type: List
   - Collection: attendance_logs
   - Group by: employee_id
   - Function: count
   - Sort: desc
   - Limit: 10

---

## 🛠️ Troubleshooting

### ESP32 không kết nối được Directus

**Check:**
1. DIRECTUS_URL có **trailing slash** `/` không? → Remove it!
2. Token có đúng không? Copy lại từ Directus
3. Role `esp32_device` có permissions chưa?
4. Directus có CORS enabled không? (nếu dùng web)

### "Device chưa được đăng ký"

**Solution:**
- ESP32 sẽ tự động đăng ký device lần đầu
- Check collection `devices` → Device MAC có tồn tại không?
- Manual tạo device với MAC address của ESP32

### "Không tìm thấy employee code"

**Solution:**
- Check collection `employees` → Employee code có tồn tại không?
- Employee code phải **chính xác** (case-sensitive)
- Tạo employee trước khi enroll fingerprint

### "Không tìm thấy fingerprint khớp"

**Lý do:**
- Template matching dùng **exact match** (simple)
- Vân tay có thể hơi khác mỗi lần scan
- Trong production nên implement **Hamming distance algorithm**

**Temporary fix:**
- Đăng ký lại vân tay
- Giảm `confidence_threshold` trong fingerprints table

---

## 🎯 Next Steps

1. **Customize device name** trong `main.cpp` line 86
2. **Add more employees** trong Directus
3. **Enroll fingerprints** qua Serial Monitor
4. **Enable auto-login mode** để test
5. **Check attendance logs** trong Directus
6. **Create dashboard** để monitoring
7. **Setup Flows** (optional) cho auto-notifications

---

## 📚 Tài liệu liên quan

- [DIRECTUS-SCHEMA.md](DIRECTUS-SCHEMA.md) - Full schema documentation
- [README.md](../README.md) - General project docs
- [Directus Docs](https://docs.directus.io/) - Official docs

---

## 💡 Tips

- **Backup Directus**: Thường xuyên export schema và data
- **Monitor logs**: Check Directus logs nếu có lỗi
- **Use Postman**: Test API endpoints trước khi code
- **Version control**: Track schema changes trong git
- **Production**: Enable HTTPS, rate limiting, monitoring
