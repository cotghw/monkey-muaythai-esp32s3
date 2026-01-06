# Code Review: Directus Client Schema Update (Phase 2)

**Date:** 2026-01-06
**Scope:** ESP32 Directus Client Module - API Endpoint & Field Name Updates
**Files:** `src/directus-client.cpp`, `src/directus-client.h`
**Plan:** `/Users/yihan/working-base/yihan/monkey-muaythai/esp32s3/plans/251224-mqtt-remote-control.md`

---

## Overall Assessment

**Quality Score:** 6/10 - Schema updates incomplete, critical inconsistencies remain

**Status:** ⚠️ **PARTIALLY COMPLETE** - Core changes done in directus-client module but downstream code not updated

---

## Scope

### Files Reviewed
- `src/directus-client.cpp` (352 lines)
- `src/directus-client.h` (130 lines)
- `src/main.cpp` (partial - lines 274-601)
- `src/command-handler.cpp` (partial - lines 40-99)

### Review Focus
Phase 2 schema alignment: API endpoint updates, field name changes, function signature consistency

---

## Critical Issues

### 1. ⚠️ **INCOMPLETE MIGRATION** - Severity: CRITICAL
**Old field names still present in downstream code**

**Evidence:**
```cpp
// src/main.cpp:397, 597
String employeeId;  // Should be memberId

// src/main.cpp:274-302, 496
String employeeCode = "";  // Variable naming inconsistent with backend

// src/command-handler.cpp:55, 67, 94
String employeeCode = params["employee_code"] | "";
resultDoc["employee_code"] = employeeCode;
```

**Impact:**
- Data flow broken between MQTT commands and Directus API
- `enrollFingerprint()` receives `employee_code` param but expects `memberId` UUID
- Variable names misleading - implies employee code strings accepted but API needs UUIDs

**Fix Required:**
```cpp
// main.cpp:397, 597
String memberId;  // Match API schema

// command-handler.cpp:55
String memberId = params["member_id"] | "";  // Match collection field name

// directus-client signature already correct (line 161):
bool enrollFingerprint(..., const String& memberId);  // ✅ Correct
```

---

### 2. ⚠️ **SEMANTIC MISMATCH** - Severity: HIGH
**Function parameter naming doesn't match data type requirements**

**Problem:**
```cpp
// directus-client.h:39
bool enrollFingerprint(const String& deviceMac, uint8_t fingerprintID,
                      const String& templateData, const String& memberId);
```

Parameter named `memberId` but old code passes `employeeCode` (string code like "EMP001")

**Backend Expectation:**
```json
// POST /items/member_fingerprints
{
  "member_id": "uuid-123-456",  // Must be UUID, NOT employee code
  "finger_print_id": 5
}
```

**Impact:**
- API will reject enroll requests with non-UUID member_id values
- Enrollment will fail silently unless error checking improved

**Fix Required:**
1. Update MQTT command schema to use `member_id` (UUID)
2. Or add lookup: `employee_code` → `member_id` via Directus query
3. Rename variables in main.cpp and command-handler.cpp

---

### 3. ⚠️ **DOCUMENTATION OUT OF SYNC** - Severity: MEDIUM
**Header file comments still reference old field names**

```cpp
// directus-client.h:61
@param reason Lý do (success, low_confidence, not_registered...)
// Should clarify: Maps to deny_reason field (line 231)

// directus-client.h:112
@param employeeId Output: Member UUID nếu tìm thấy
// Correct name but inconsistent with function signature using 'memberId'
```

---

## High Priority Findings

### 4. ⚠️ **HARDCODED CREDENTIALS** - Severity: HIGH (Security)
**Production secrets exposed in config.h**

```cpp
// src/config.h:7-8
#define WIFI_SSID "SAN SAN"
#define WIFI_PASSWORD "0399492511"

// src/config.h:14-15
#define DIRECTUS_URL "https://optimization-kind-diverse-appeared.trycloudflare.com"
#define DIRECTUS_TOKEN "ux--xiAf1QRI7PcVXdTlNw1FhblowiwU"
```

**Risks:**
- WiFi password exposed (phone number format)
- Directus static token grants full admin access
- Cloudflare tunnel URL reveals temporary test infrastructure

**Fix Required:**
1. Create `config.example.h` template with placeholders
2. Add `config.h` to `.gitignore`
3. Rotate exposed credentials immediately
4. Use per-device tokens (not static admin token)

---

### 5. ⚠️ **MISSING VALIDATION** - Severity: HIGH
**No field validation before API calls**

```cpp
// directus-client.cpp:191-195
enrollDoc["member_id"] = memberId;
enrollDoc["device_id"] = deviceId;
enrollDoc["finger_print_id"] = fingerprintID;
```

No checks:
- `memberId` is valid UUID format (36 chars)
- `deviceId` non-empty
- `fingerprintID` in range 1-127

**Impact:**
- Invalid data sent to API causes HTTP 400 errors
- Poor user feedback (generic "enrollment failed")

**Fix:**
```cpp
// Add validation
if (memberId.length() != 36 || memberId.indexOf('-') == -1) {
    Serial.printf("✗ Invalid member_id format: %s\n", memberId.c_str());
    return false;
}
```

---

### 6. ⚠️ **FIELD INCONSISTENCY** - Severity: MEDIUM
**Schema update applied inconsistently across functions**

✅ **Correct:**
```cpp
// Line 24, 58, 74: fingerprint_devices ✅
// Line 179: /items/members ✅
// Line 193: finger_print_id ✅
// Line 206: member_fingerprints ✅
// Line 231: deny_reason ✅
// Line 239: check_in_time ✅
// Line 242: /items/attendance ✅
```

❌ **Still Wrong:**
```cpp
// main.cpp:397, 597: employeeId variable name
// command-handler.cpp:55, 94: employee_code param name
```

---

## Medium Priority Improvements

### 7. 🔵 **TYPE SAFETY** - Code Quality
**Missing const correctness and stronger typing**

```cpp
// directus-client.cpp:92
bool DirectusClient::compareTemplates(const String& template1, const String& template2) {
    return template1 == template2;
}
```

Simple string comparison insufficient for fingerprint matching. Comment acknowledges this (line 94) but no implementation plan.

**Recommendation:**
```cpp
// Add confidence threshold parameter
bool compareTemplates(const String& t1, const String& t2, float threshold = 0.85);
// Or mark as deprecated, rely on R307 sensor matching only
```

---

### 8. 🔵 **ERROR HANDLING** - Robustness
**Inconsistent error return patterns**

```cpp
// directus-client.cpp:265-267
if (deviceId.length() == 0) {
    Serial.println("✗ Device chưa được đăng ký!");
    return 0;
}
```

Returns `0` for error but no distinction from "0 fingerprints found".

**Better:**
```cpp
// Return -1 for error, 0 for empty, >0 for count
int getFingerprints(...) {
    if (!_wifiManager->isConnected()) return -1;
    if (deviceId.length() == 0) return -1;
    // ... valid result 0-127
}
```

---

### 9. 🔵 **MEMORY EFFICIENCY** - Performance
**Unnecessary JsonDocument allocations**

```cpp
// directus-client.cpp:190
JsonDocument enrollDoc;
```

Small fixed-size docs can use `StaticJsonDocument<256>` to avoid heap fragmentation.

**Recommendation:**
```cpp
StaticJsonDocument<512> enrollDoc;  // Enrollment has few fields
StaticJsonDocument<1024> doc;       // For API responses
```

---

### 10. 🔵 **MAGIC NUMBERS** - Maintainability
**Hardcoded confidence threshold**

```cpp
// directus-client.cpp:136
const uint16_t MIN_CONFIDENCE = 80;
```

Should be in `config.h` for easy tuning:
```cpp
#define MIN_CONFIDENCE_THRESHOLD 80
```

---

## Low Priority Suggestions

### 11. 🟢 **CODE FORMATTING** - Style
Vietnamese comments mixed with English code - consider standardizing:

```cpp
// Current:
Serial.println("✓ Đã log attendance");  // Mixed

// Better:
Serial.println("✓ Attendance logged");  // English
// Or全Vietnamese: Serial.println("✓ Đã ghi nhận điểm danh");
```

---

### 12. 🟢 **TEMPLATE SIZE PADDING** - Logic Clarity
**Unclear why padding to 512 bytes**

```cpp
// directus-client.cpp:338-341
if (*templateSize < 512) {
    memset(templateBuffer + *templateSize, 0, 512 - *templateSize);
    *templateSize = 512;
}
```

R307 templates may be variable size. Padding might be sensor requirement - add comment explaining why.

---

## Positive Observations

✅ **Good API Consistency:**
- All 4 collection endpoints updated correctly
- Field name changes applied systematically in directus-client module

✅ **Comprehensive Functions:**
- Good separation: verify, enroll, register, log, download
- Each function has clear responsibility (YAGNI compliant)

✅ **Error Messages:**
- Vietnamese messages useful for local developers
- Emoji prefixes (✓/✗) improve readability

✅ **Base64 Handling:**
- Using mbedtls for decode (line 318) - secure standard library

✅ **Caching:**
- Device UUID cached after first fetch (line 19-21) - reduces API calls

---

## YAGNI/KISS/DRY Assessment

### YAGNI Violations
❌ `compareTemplates()` function (line 92) - Not used, TODO comment suggests future use
- **Fix:** Remove or implement properly

### KISS Compliance
✅ Simple HTTP GET/POST wrappers
✅ Straightforward JSON serialization
✅ No over-engineering

### DRY Violations
⚠️ Timestamp generation duplicated (lines 198-203, 234-239)
- **Fix:** Extract to helper function:
```cpp
String getCurrentISOTimestamp() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    char buf[30];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S.000Z", &timeinfo);
    return String(buf);
}
```

---

## Security Audit

### 🔴 Critical Security Issues
1. **Hardcoded credentials** (config.h) - HIGH RISK
2. **No input sanitization** for memberId, deviceMac - SQL injection risk if backend vulnerable
3. **Static admin token** instead of per-device tokens - Lateral movement risk

### 🟡 Medium Security Issues
1. **No TLS certificate validation** - MITM risk (WiFiClient default)
2. **Template data transmitted as Base64** - Not encrypted, visible in logs

### Recommendations
```cpp
// Use WiFiClientSecure with certificate pinning
WiFiClientSecure _wifiClient;
_wifiClient.setCACert(DIRECTUS_CA_CERT);

// Validate UUIDs before API calls
bool isValidUUID(const String& uuid) {
    return (uuid.length() == 36 && uuid.charAt(8) == '-' && uuid.charAt(13) == '-');
}
```

---

## Task Completeness (Phase 2)

### ✅ Completed
- [x] Update API endpoints: `fingerprint_devices`, `member_fingerprints`, `attendance`, `members`
- [x] Update field names: `member_id`, `finger_print_id`, `status=active`, `deny_reason`, `check_in_time`
- [x] Function signatures updated in directus-client.h

### ❌ Incomplete
- [ ] Update main.cpp variable names (`employeeId` → `memberId`)
- [ ] Update command-handler.cpp MQTT param names (`employee_code` → `member_id`)
- [ ] Update header documentation to match new field names
- [ ] Add validation for UUID format inputs
- [ ] Test end-to-end: MQTT command → Directus API

---

## Recommended Actions (Priority Order)

1. **CRITICAL (Block deployment):**
   - Fix variable naming in main.cpp (lines 397, 597)
   - Fix MQTT param in command-handler.cpp (line 55, 94)
   - Move credentials to config.example.h + .gitignore
   - Rotate exposed WiFi/Directus credentials

2. **HIGH (Fix before next feature):**
   - Add UUID validation before API calls
   - Extract timestamp helper function (DRY)
   - Update header file documentation
   - Test enrollment flow end-to-end

3. **MEDIUM (Next sprint):**
   - Implement error codes (-1 for error, 0+ for results)
   - Use StaticJsonDocument for fixed-size objects
   - Move MIN_CONFIDENCE to config.h
   - Remove or implement compareTemplates()

4. **LOW (Backlog):**
   - Standardize comment language (English or Vietnamese)
   - Add comment explaining 512-byte padding
   - Implement TLS certificate validation

---

## Metrics

- **Lines Changed:** ~60 lines (API endpoints + field names)
- **Functions Affected:** 7/11 (verifyFingerprint, enrollFingerprint, logAttendance, getFingerprints, downloadFingerprintTemplate, registerDevice, getDeviceId)
- **Breaking Changes:** Yes - requires backend schema update + dependent code changes
- **Test Coverage:** 0% (no automated tests found)
- **Build Status:** ⚠️ Unknown (pio command not available)

---

## Unresolved Questions

1. **UUID vs Employee Code:** Should MQTT command use `member_id` (UUID) or lookup via `employee_code`? Impacts command schema design.

2. **Template Matching:** Line 100-111 skips template matching - is R307 sensor matching sufficient or should Directus do secondary verification?

3. **Field Transition Plan:** Are old field names (`employee_id`, `fingerprint_id`) still supported in backend for migration period?

4. **Confidence Threshold:** MIN_CONFIDENCE=80 - is this empirically validated or placeholder? Should be environment-specific?

5. **512-byte Padding:** Why pad templates to exactly 512 bytes? R307 spec requirement or Directus storage format?

---

## Next Steps

**Before merging:**
1. Complete variable renaming in main.cpp and command-handler.cpp
2. Test full flow: MQTT enroll command → Directus API → verify attendance log created
3. Update MQTT command schema documentation to reflect `member_id` requirement

**Before production:**
1. Implement credential protection (config.example.h + .gitignore)
2. Rotate all exposed credentials
3. Add UUID validation with proper error messages
4. Test with invalid inputs (empty UUID, out-of-range fingerprint ID)

---

**Review Status:** ⚠️ **CHANGES REQUIRED**
**Reviewer Confidence:** HIGH (schema changes verified, inconsistencies confirmed)
**Recommended Action:** Fix critical naming issues before proceeding to Phase 3
