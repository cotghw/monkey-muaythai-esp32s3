#ifndef FINGERPRINT_HANDLER_H
#define FINGERPRINT_HANDLER_H

#include <Adafruit_Fingerprint.h>

// Forward declaration
class BuzzerHandler;

// Trạng thái xử lý vân tay
enum FingerprintStatus {
    FP_SUCCESS,          // Thành công
    FP_NO_FINGER,        // Không có ngón tay
    FP_IMAGE_FAIL,       // Lỗi lấy ảnh
    FP_MATCH_FAIL,       // Không khớp
    FP_ENROLL_FAIL,      // Lỗi đăng ký
    FP_DELETE_FAIL,      // Lỗi xóa
    FP_COMMUNICATION_ERROR  // Lỗi kết nối
};

class FingerprintHandler {
private:
    Adafruit_Fingerprint *finger;
    HardwareSerial *serialPort;  // Serial port cho raw UART access
    uint8_t enrollStep;  // Bước đăng ký hiện tại
    BuzzerHandler* buzzer;    // Buzzer

public:
    FingerprintHandler(HardwareSerial *serial);

    // Khởi tạo và kiểm tra kết nối
    bool begin();

    // Set buzzer cho UX feedback
    void setBuzzer(BuzzerHandler* buzz);

    // Lấy thông tin cảm biến
    void printSensorInfo();
    uint16_t getTemplateCount();

    // Đăng ký vân tay (trả về ID nếu thành công, -1 nếu thất bại)
    int enrollFingerprint(uint8_t id);

    // Xác thực vân tay (trả về ID nếu tìm thấy, -1 nếu không tìm thấy)
    int verifyFingerprint();

    // Xóa vân tay
    bool deleteFingerprint(uint8_t id);
    bool deleteAllFingerprints();

    // Kiểm tra ngón tay có đặt trên cảm biến không
    bool isFingerDetected();

    // Lấy ảnh vân tay
    FingerprintStatus captureImage();

    // Lấy fingerprint template data (raw data của vân tay)
    bool getTemplate(uint8_t id, uint8_t* templateBuffer, uint16_t* templateSize);

    // Download template từ sensor buffer sang storage
    bool downloadModel(uint8_t slot, uint8_t* templateBuffer, uint16_t* templateSize);

    // Upload template từ buffer lên sensor
    bool uploadModel(uint8_t id, uint8_t* templateBuffer, uint16_t templateSize);

    // Lấy confidence score từ lần verify cuối
    uint16_t getConfidence();

    // LED control
    void ledOn(uint8_t color);  // 1=Red, 2=Blue, 3=Purple
    void ledOff();
};

#endif
