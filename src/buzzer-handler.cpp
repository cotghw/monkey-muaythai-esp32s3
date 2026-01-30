#include "buzzer-handler.h"
#include <Arduino.h>

BuzzerHandler::BuzzerHandler() : initialized(false), enabled(true) {
}

bool BuzzerHandler::begin() {
    // KY-012 Active Buzzer - chỉ cần digitalWrite, không cần PWM
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);  // Start off

    initialized = true;
    Serial.println("✓ Buzzer KY-012 sẵn sàng!");
    return true;
}

void BuzzerHandler::setEnabled(bool enabled) {
    this->enabled = enabled;
    if (!enabled) {
        stop();
    }
}

bool BuzzerHandler::isEnabled() {
    return enabled;
}

void BuzzerHandler::play(BuzzerSound sound) {
    if (!initialized || !enabled) return;

    switch (sound) {
        case BUZZ_SUCCESS:
            beep(100);
            delay(150);
            beep(100);
            break;

        case BUZZ_ERROR:
            beep(500);
            break;

        case BUZZ_WARNING:
            beep(200);
            break;

        case BUZZ_SINGLE:
            beep(100);
            break;

        case BUZZ_DOUBLE:
            beep(100);
            delay(150);
            beep(100);
            break;

        case BUZZ_CONFIRM:
            beep(80);
            delay(100);
            beep(80);
            delay(100);
            beep(80);
            break;

        case BUZZ_ENROLL_WAIT:
            beep(50);
            delay(500);
            beep(50);
            break;

        case BUZZ_ACCESS_GRANTED:
            playSuccessMelody();
            break;

        case BUZZ_ACCESS_DENIED:
            playErrorMelody();
            break;

        case BUZZ_DELETE:
            beep(200);
            break;
    }
}

void BuzzerHandler::beep(int durationMs) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(durationMs);
    digitalWrite(BUZZER_PIN, LOW);
}

void BuzzerHandler::beep(int frequency, int durationMs) {
    // KY-012 active buzzer - ignore frequency, chỉ dùng duration
    beep(durationMs);
}

void BuzzerHandler::beep(int frequency, int onMs, int offMs, int count) {
    for (int i = 0; i < count; i++) {
        beep(onMs);
        if (i < count - 1) {
            delay(offMs);
        }
    }
}

void BuzzerHandler::playSuccessMelody() {
    // Happy melody dài: ascend lên
    beep(100);
    delay(100);
    beep(100);
    delay(100);
    beep(100);
    delay(100);
    beep(150);
    delay(100);
    beep(200);  // Note cao cuối
}

void BuzzerHandler::playErrorMelody() {
    // Error ngắn: 2 buzz
    beep(150);
    delay(100);
    beep(150);
}

void BuzzerHandler::stop() {
    if (initialized) {
        digitalWrite(BUZZER_PIN, LOW);
    }
}
