#ifndef OFFLINE_QUEUE_H
#define OFFLINE_QUEUE_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "http-client.h"

#define QUEUE_DIR "/queue"
#define MAX_QUEUE_SIZE 100

struct QueueEntry {
    String endpoint;
    String method;  // "POST" or "PATCH"
    String payload;
    uint32_t timestamp;
    uint8_t retries;
};

class OfflineQueue {
public:
    OfflineQueue();

    bool begin();
    bool enqueue(const String& endpoint, const String& method, const String& payload);
    bool dequeue(QueueEntry& entry);
    bool remove(const String& filename);
    int getPendingCount();
    void flush(HTTPClientManager* http, const String& baseUrl);
    void clear();

private:
    bool _initialized;
    String getNextFilename();
    String getOldestFilename();
    bool writeEntry(const String& filename, const QueueEntry& entry);
    bool readEntry(const String& filename, QueueEntry& entry);
};

#endif
