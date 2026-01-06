#include "offline-queue.h"

OfflineQueue::OfflineQueue() : _initialized(false) {}

bool OfflineQueue::begin() {
    if (!LittleFS.begin(true)) {  // true = format if failed
        Serial.println("[QUEUE] LittleFS mount failed");
        return false;
    }

    // Create queue directory if not exists
    if (!LittleFS.exists(QUEUE_DIR)) {
        LittleFS.mkdir(QUEUE_DIR);
    }

    _initialized = true;
    Serial.printf("[QUEUE] Initialized, %d pending entries\n", getPendingCount());
    return true;
}

bool OfflineQueue::enqueue(const String& endpoint, const String& method,
                           const String& payload) {
    if (!_initialized) return false;

    if (getPendingCount() >= MAX_QUEUE_SIZE) {
        Serial.println("[QUEUE] Queue full, dropping oldest entry");
        String oldest = getOldestFilename();
        if (oldest.length() > 0) {
            LittleFS.remove(oldest);
        }
    }

    QueueEntry entry;
    entry.endpoint = endpoint;
    entry.method = method;
    entry.payload = payload;
    entry.timestamp = millis() / 1000;
    entry.retries = 0;

    String filename = getNextFilename();
    if (writeEntry(filename, entry)) {
        Serial.printf("[QUEUE] Enqueued: %s\n", endpoint.c_str());
        return true;
    }
    return false;
}

bool OfflineQueue::dequeue(QueueEntry& entry) {
    if (!_initialized) return false;

    String filename = getOldestFilename();
    if (filename.length() == 0) return false;

    return readEntry(filename, entry);
}

bool OfflineQueue::remove(const String& filename) {
    return LittleFS.remove(filename);
}

int OfflineQueue::getPendingCount() {
    if (!_initialized) return 0;

    int count = 0;
    File root = LittleFS.open(QUEUE_DIR);
    if (!root) return 0;

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) count++;
        file = root.openNextFile();
    }
    return count;
}

void OfflineQueue::flush(HTTPClientManager* http, const String& baseUrl) {
    if (!_initialized) return;

    int count = getPendingCount();
    if (count == 0) return;

    Serial.printf("[QUEUE] Flushing %d entries...\n", count);

    int success = 0, failed = 0;

    while (true) {
        String filename = getOldestFilename();
        if (filename.length() == 0) break;

        QueueEntry entry;
        if (!readEntry(filename, entry)) {
            remove(filename);
            continue;
        }

        String url = baseUrl + entry.endpoint;
        String response;
        int httpCode;

        if (entry.method == "POST") {
            httpCode = http->post(url.c_str(), entry.payload, response);
        } else {
            // TODO: Add PATCH support
            httpCode = -1;
        }

        if (httpCode == 200 || httpCode == 201) {
            success++;
            remove(filename);
        } else {
            entry.retries++;
            if (entry.retries >= 3) {
                Serial.printf("[QUEUE] Max retries reached, dropping: %s\n",
                             entry.endpoint.c_str());
                remove(filename);
                failed++;
            } else {
                writeEntry(filename, entry);
                failed++;
                break;  // Stop on first failure to preserve order
            }
        }

        delay(100);  // Rate limiting
    }

    Serial.printf("[QUEUE] Flush complete: %d success, %d failed\n", success, failed);
}

void OfflineQueue::clear() {
    if (!_initialized) return;

    File root = LittleFS.open(QUEUE_DIR);
    if (!root) return;

    File file = root.openNextFile();
    while (file) {
        String path = String(QUEUE_DIR) + "/" + file.name();
        file = root.openNextFile();
        LittleFS.remove(path);
    }

    Serial.println("[QUEUE] Cleared all entries");
}

String OfflineQueue::getNextFilename() {
    // Find highest numbered file and add 1
    int maxNum = 0;
    File root = LittleFS.open(QUEUE_DIR);
    if (root) {
        File file = root.openNextFile();
        while (file) {
            String name = file.name();
            int num = name.toInt();
            if (num > maxNum) maxNum = num;
            file = root.openNextFile();
        }
    }

    char filename[32];
    snprintf(filename, sizeof(filename), "%s/%06d.json", QUEUE_DIR, maxNum + 1);
    return String(filename);
}

String OfflineQueue::getOldestFilename() {
    int minNum = INT_MAX;
    String oldest = "";

    File root = LittleFS.open(QUEUE_DIR);
    if (!root) return "";

    File file = root.openNextFile();
    while (file) {
        String name = file.name();
        int num = name.toInt();
        if (num < minNum) {
            minNum = num;
            oldest = String(QUEUE_DIR) + "/" + name;
        }
        file = root.openNextFile();
    }
    return oldest;
}

bool OfflineQueue::writeEntry(const String& filename, const QueueEntry& entry) {
    File file = LittleFS.open(filename, "w");
    if (!file) return false;

    JsonDocument doc;
    doc["endpoint"] = entry.endpoint;
    doc["method"] = entry.method;
    doc["payload"] = entry.payload;
    doc["timestamp"] = entry.timestamp;
    doc["retries"] = entry.retries;

    serializeJson(doc, file);
    file.close();
    return true;
}

bool OfflineQueue::readEntry(const String& filename, QueueEntry& entry) {
    File file = LittleFS.open(filename, "r");
    if (!file) return false;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) return false;

    entry.endpoint = doc["endpoint"].as<String>();
    entry.method = doc["method"].as<String>();
    entry.payload = doc["payload"].as<String>();
    entry.timestamp = doc["timestamp"] | 0;
    entry.retries = doc["retries"] | 0;

    return true;
}
