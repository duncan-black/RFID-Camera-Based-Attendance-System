# Hệ thống Điểm danh Thông minh (ESP32-S3-CAM + RFID)

Hệ thống điểm danh tự động kết hợp nhận diện thẻ RFID và xác thực bằng hình ảnh. Khi người dùng quẹt thẻ, thiết bị sẽ tự động chụp ảnh và gửi dữ liệu về server để lưu trữ và hiển thị.

## 1. Mô tả
Hệ thống sử dụng **ESP32-S3-CAM** làm bộ xử lý trung tâm, giao tiếp với đầu đọc thẻ **RC522** để xác định danh tính và camera **OV2640** để chụp ảnh khuôn mặt. Dữ liệu (UID thẻ + ảnh) được gửi về server **Flask** để lưu trữ và hiển thị trên dashboard thời gian thực.

## 2. Tính năng nổi bật
* **Firmware đa nhiệm:** Xử lý đồng thời Camera, RFID, WiFi và MQTT trên một vi điều khiển duy nhất.
* **Kiến trúc Non-blocking:** Tách biệt các tác vụ nặng (chụp ảnh, truyền tải HTTP) để hệ thống luôn ổn định, không bị treo.
* **Kết nối bảo mật:** Sử dụng MQTT qua TLS (HiveMQ Cloud) để giám sát trạng thái thiết bị từ xa.
* **Backend đầy đủ:** Tích hợp sẵn Web Server (Flask) và Database (SQLite) để quản lý lịch sử điểm danh và đăng ký người dùng.
* **Full-stack IoT:** Giải pháp trọn gói từ nhúng (C++) đến web (Python/HTML/CSS).

## 3. Sơ đồ phần cứng
Dưới đây là bảng kết nối chân giữa các module với ESP32-S3-CAM:

| Module | Chân (Module) | Chân (ESP32-S3 GPIO) |
| :--- | :--- | :--- |
| **RC522 (RFID)** | SDA (SS/CS) | GPIO 1 |
| | SCK | GPIO 2 |
| | MOSI | GPIO 3 |
| | MISO | GPIO 14 |
| | RST | GPIO 21 |
| | VCC | 3.3V |
| | GND | GND |
| **Camera** | *Sử dụng bus song song* | GPIO 4-13, 15-18 |

> **⚠️ Lưu ý:** Các chân từ 4 đến 18 được dành riêng cho Camera, không sử dụng cho các thiết bị ngoại vi khác. Hãy kiểm tra sơ đồ chân (pinout) trên board thực tế của bạn vì mỗi nhà sản xuất có thể có cấu hình khác nhau.

## 4. Hướng dẫn cài đặt nhanh
1. **Phần cứng:** Kết nối dây theo bảng trên và cấp nguồn 3.3V ổn định.
2. **Firmware (Arduino IDE):**
   * Cài đặt Board ESP32 (bản 2.0.14+).
   * Cài các thư viện: `PubSubClient`, `ArduinoJson`, `MFRC522`.
   * Cấu hình Flash: 16MB Flash, OPI PSRAM (bắt buộc cho bản N16R8).
   * Điền thông tin WiFi, MQTT và IP Server vào code.
3. **Server (Python/Flask):**
   * Cài đặt thư viện: `pip install -r requirements.txt`.
   * Chạy server: `python server.py`.
   * Truy cập `http://<IP_Server>:5000` để xem dashboard.# RFID-Camera-Based-Attendance-System
