#!/bin/bash

# Script test nhanh R307 trên ESP32-S3

echo "╔═══════════════════════════════════════════╗"
echo "║   QUICK TEST R307 + ESP32-S3              ║"
echo "╚═══════════════════════════════════════════╝"
echo ""

# Kiểm tra đang ở đúng thư mục
if [ ! -f "platformio.ini" ]; then
    echo "❌ Lỗi: Chạy script này từ thư mục esp32s3"
    exit 1
fi

echo "Chọn chế độ test:"
echo "1. Test code chính (main.cpp)"
echo "2. Test UART cơ bản (test-uart-basic.cpp)"
echo ""
read -p "Chọn (1/2): " choice

case $choice in
    1)
        echo ""
        echo "→ Sử dụng code chính..."
        # Đảm bảo main.cpp đang được dùng
        if [ -f "src/main.cpp.bak" ]; then
            mv src/main.cpp.bak src/main.cpp
        fi
        ;;
    2)
        echo ""
        echo "→ Chuyển sang code test UART..."
        # Backup main.cpp nếu chưa backup
        if [ ! -f "src/main.cpp.bak" ]; then
            cp src/main.cpp src/main.cpp.bak
        fi
        # Copy test code vào src
        cp test/test-uart-basic.cpp src/main.cpp
        echo "✓ Đã chuyển sang test-uart-basic.cpp"
        ;;
    *)
        echo "❌ Lựa chọn không hợp lệ"
        exit 1
        ;;
esac

echo ""
echo "→ Build và upload code..."
pio run -t upload

if [ $? -ne 0 ]; then
    echo ""
    echo "❌ Upload thất bại!"
    echo "Kiểm tra:"
    echo "  - ESP32 đã kết nối USB"
    echo "  - Driver CH340/CP210x đã cài"
    echo "  - Không có app khác đang dùng serial port"
    exit 1
fi

echo ""
echo "✓ Upload thành công!"
echo ""
echo "→ Mở Serial Monitor (baud 115200)..."
echo "→ Nhấn Ctrl+C để thoát"
echo ""

sleep 2
pio device monitor
