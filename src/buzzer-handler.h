#ifndef BUZZER_HANDLER_H
#define BUZZER_HANDLER_H

#include "config.h"

// Loại âm thanh
enum BuzzerSound {
    BUZZ_SUCCESS,      // Thành công - 2 beep ngắn, cao
    BUZZ_ERROR,        // Lỗi - 1 beep dài, thấp
    BUZZ_WARNING,      // Cảnh báo - 1 beep trung bình
    BUZZ_SINGLE,       // 1 beep ngắn
    BUZZ_DOUBLE,       // 2 beep ngắn
    BUZZ_CONFIRM,      // Xác nhận - 3 beep
    BUZZ_ENROLL_WAIT,  // Đợi enroll - beep liên tục
    BUZZ_ACCESS_GRANTED, // Truy cập được phép - melody
    BUZZ_ACCESS_DENIED,  // Truy cập bị từ chối - buzz dài
    BUZZ_DELETE        // Xóa - 1 beep
};

class BuzzerHandler {
private:
    bool initialized;
    bool enabled;  // Cho phép tắt tiếng nếu cần

public:
    BuzzerHandler();

    // Khởi tạo buzzer
    bool begin();

    // Tắt/bật tiếng
    void setEnabled(bool enabled);
    bool isEnabled();

    // Phát âm thanh
    void play(BuzzerSound sound);
    void beep(int durationMs);  // KY-012 active buzzer - chỉ cần duration
    void beep(int frequency, int durationMs);  // Không dùng cho active buzzer
    void beep(int frequency, int onMs, int offMs, int count);

    // Melody cho access granted
    void playSuccessMelody();
    void playErrorMelody();

    // Dừng âm thanh
    void stop();
};

#endif
