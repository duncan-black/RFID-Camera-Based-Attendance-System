/*
 * ESP32-S3-CAM (N16R8) - Attendance System
 * Arduino IDE version - MQTT + HTTP upload
 *
 * Thư viện cần cài qua Library Manager:
 *   - PubSubClient (Nick O'Leary)
 *   - ArduinoJson (Benoit Blanchon), bản 7.x
 *
 * Tools menu cần set:
 *   Board: ESP32S3 Dev Module
 *   Flash Size: 16MB
 *   Partition Scheme:  16M Flash
 *   PSRAM: OPI PSRAM 
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>
#include "esp_camera.h"
#include "camera_pins.h"

//CẤU HÌNH RFID (RC522) 

#define RFID_SS_PIN   1
#define RFID_SCK_PIN  2
#define RFID_MOSI_PIN 3
#define RFID_MISO_PIN 14
#define RFID_RST_PIN  21

MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);

// Debounce: không đọc lại cùng 1 thẻ trong khoảng thời gian này (ms)
const unsigned long RFID_DEBOUNCE_MS = 4000;
String        lastUid = "";
unsigned long lastScanTime = 0;


const char* WIFI_SSID     = "chungcu";
const char* WIFI_PASSWORD = "123454326";


const char* MQTT_BROKER = "846bba1bfee147cb8cead11924b8c6af.s1.eu.hivemq.cloud";   
const int   MQTT_PORT   = 8883;                           


const char* MQTT_USER     = "kikuka";
const char* MQTT_PASSWORD = "1234Kiet";


const char* MQTT_TOPIC_COMMAND = "esp32cam_attendance/command";
const char* MQTT_TOPIC_STATUS  = "esp32cam_attendance/status";
const char* MQTT_TOPIC_RESULT  = "esp32cam_attendance/result";

//  CẤU HÌNH SERVER 
const char* SERVER_URL = "http://10.79.213.132:5000/upload";

// BIẾN TOÀN CỤC 
WiFiClientSecure espClient;
PubSubClient     mqttClient(espClient);

volatile bool captureRequested = false;
String        pendingUid = "";

unsigned long lastMqttReconnectAttempt = 0;


bool initCamera() {
    camera_config_t config;
    config.pin_pwdn     = CAM_PIN_PWDN;
    config.pin_reset    = CAM_PIN_RESET;
    config.pin_xclk     = CAM_PIN_XCLK;
    config.pin_sccb_sda = CAM_PIN_SIOD;
    config.pin_sccb_scl = CAM_PIN_SIOC;
    config.pin_d7       = CAM_PIN_D7;
    config.pin_d6       = CAM_PIN_D6;
    config.pin_d5       = CAM_PIN_D5;
    config.pin_d4       = CAM_PIN_D4;
    config.pin_d3       = CAM_PIN_D3;
    config.pin_d2       = CAM_PIN_D2;
    config.pin_d1       = CAM_PIN_D1;
    config.pin_d0       = CAM_PIN_D0;
    config.pin_vsync    = CAM_PIN_VSYNC;
    config.pin_href     = CAM_PIN_HREF;
    config.pin_pclk     = CAM_PIN_PCLK;

    config.xclk_freq_hz = 20000000; 
    config.ledc_timer   = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;

    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size   = FRAMESIZE_SVGA;   // 800x600
    config.jpeg_quality = 12;

    config.fb_count     = 1;                // chỉ chụp 
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[CAMERA] Init failed, error: 0x%x\n", err);
        return false;
    }
    Serial.println("[CAMERA] Initialized (SVGA)");
    return true;
}

// KHỞI TẠO RFID

void initRFID() {
    // Dùng object SPI toàn cục (chính là object thư viện MFRC522 tham chiếu tới bên trong).
    // begin() với 4 tham số cho phép remap chân trên ESP32/ESP32-S3.
    SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);
    mfrc522.PCD_Init();
    delay(50);

    // Kiểm tra module có phản hồi không (đọc version register)
    byte version = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
    if (version == 0x00 || version == 0xFF) {
        Serial.println("[RFID] WARNING: RC522 not responding. Check wiring.");
    } else {
        Serial.printf("[RFID] RC522 detected, version: 0x%02X\n", version);
    }
}

// Chuyển UID dạng byte sang chuỗi hex
String uidToString(MFRC522::Uid uid) {
    String result = "";
    for (byte i = 0; i < uid.size; i++) {
        if (uid.uidByte[i] < 0x10) result += "0";
        result += String(uid.uidByte[i], HEX);
    }
    result.toUpperCase();
    return result;
}

// Đọc thẻ RFID (gọi trong loop(), non-blocking, có debounce)
void checkRFID() {
    if (!mfrc522.PICC_IsNewCardPresent()) return;
    if (!mfrc522.PICC_ReadCardSerial()) return;

    String uid = uidToString(mfrc522.uid);
    unsigned long now = millis();

    // Debounce: bỏ qua nếu quẹt lại cùng thẻ trong thời gian ngắn
    if (uid == lastUid && (now - lastScanTime) < RFID_DEBOUNCE_MS) {
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        return;
    }

    lastUid = uid;
    lastScanTime = now;

    Serial.printf("[RFID] Card detected, UID: %s\n", uid.c_str());

    // Trigger chụp ảnh giống hệt cơ chế MQTT command cũ
    pendingUid = uid;
    captureRequested = true;

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
}


// GỬI ẢNH QUA HTTP POST
void sendImageViaHttp(camera_fb_t* fb, const String& uid) {
    HTTPClient http;
    String url = String(SERVER_URL) + "?uid=" + uid;

    http.begin(url);
    http.addHeader("Content-Type", "image/jpeg");
    http.setTimeout(10000);

    int httpCode = http.POST(fb->buf, fb->len);

    if (httpCode == 200) {
        Serial.printf("[HTTP] Upload success (UID: %s)\n", uid.c_str());

        StaticJsonDocument<128> doc;
        doc["uid"]    = uid;
        doc["status"] = "uploaded";
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(MQTT_TOPIC_RESULT, payload.c_str());
    } else {
        Serial.printf("[HTTP] POST failed, code: %d\n", httpCode);

        StaticJsonDocument<128> doc;
        doc["uid"]    = uid;
        doc["status"] = "failed";
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(MQTT_TOPIC_RESULT, payload.c_str());
    }

    http.end();
}


//XỬ LÝ CHỤP ẢNH (gọi từ loop(), KHÔNG gọi trực tiếp trong callback MQTT)

void processCapture(const String& uid) {
    Serial.printf("[CAPTURE] Processing for UID: %s\n", uid.c_str());

    // Chụp bỏ 1 khung "warm-up" -- khung đầu tiên sau thời gian camera idle
    // thường chưa ổn định (auto exposure/white balance), dễ bị lỗi cấu trúc JPEG.
    camera_fb_t* warmup = esp_camera_fb_get();
    if (warmup) {
        esp_camera_fb_return(warmup);
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("[CAPTURE] Failed to get frame buffer");
        return;
    }

    Serial.printf("[CAPTURE] Image size: %u bytes\n", fb->len);
    sendImageViaHttp(fb, uid);
    esp_camera_fb_return(fb);
}


//   MQTT CALLBACK — chỉ set flag

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message;
    message.reserve(length + 1);
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    Serial.printf("[MQTT] Received: %s\n", message.c_str());

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        Serial.printf("[MQTT] Invalid JSON: %s\n", error.c_str());
        return;
    }

    if (!doc.containsKey("cmd") || !doc["cmd"].is<const char*>()) {
        Serial.println("[MQTT] Missing or invalid 'cmd' field");
        return;
    }

    const char* cmd = doc["cmd"];
    if (strcmp(cmd, "capture") == 0) {
        const char* uid = doc["uid"] | "unknown";
        pendingUid = String(uid);
        captureRequested = true;
        Serial.printf("[MQTT] Capture enqueued for UID: %s\n", uid);
    } else {
        Serial.printf("[MQTT] Unknown command: %s\n", cmd);
    }
}

//
//KẾT NỐI / TÁI KẾT NỐI MQTT
// 
bool reconnectMqtt() {
    // esp_random() dùng RNG phần cứng của ESP32, không cần seed
    uint32_t random_num = (esp_random() % 9000) + 1000;
    String clientId = "ESP32CAM-" + WiFi.macAddress() + "-" + String(random_num);
    Serial.print("[MQTT] Connecting with clientId: ");
    Serial.println(clientId);

    bool connected;
    if (strlen(MQTT_USER) > 0) {
        connected = mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
    } else {
        connected = mqttClient.connect(clientId.c_str());
    }

    if (connected) {
        Serial.println(" connected");
        mqttClient.subscribe(MQTT_TOPIC_COMMAND);

        StaticJsonDocument<128> doc;
        doc["status"] = "online";
        doc["ip"]     = WiFi.localIP().toString();
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(MQTT_TOPIC_STATUS, payload.c_str());

        return true;
    } else {
        Serial.printf(" failed, rc=%d\n", mqttClient.state());
        return false;
    }
}


void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[SYSTEM] Starting Attendance System...");

    // ---- WiFi ----
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[WIFI] Connecting");

    int wifiRetry = 0;
    while (WiFi.status() != WL_CONNECTED && wifiRetry < 40) {
        delay(500);
        Serial.print(".");
        wifiRetry++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[WIFI] Failed to connect. Restarting in 5s...");
        delay(5000);
        ESP.restart();
    }
    Serial.printf("\n[WIFI] Connected, IP: %s\n", WiFi.localIP().toString().c_str());

    // ---- Camera ----
    if (!initCamera()) {
        Serial.println("[SYSTEM] Camera init failed. Restarting in 3s...");
        delay(3000);
        ESP.restart();
    }

    // ---- RFID ----
    initRFID();

    // ---- MQTT (TLS) ----
    espClient.setInsecure();   // bỏ qua xác thực chứng chỉ CA — xem ghi chú bảo mật bên dưới
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);   // tăng buffer cho payload JSON dài hơn mặc định (256)

    reconnectMqtt();

    Serial.println("[SYSTEM] Ready, waiting for MQTT commands...\n");
}


//                        LOOP

void loop() {
    // Giữ kết nối WiFi (không bắt buộc nhưng an toàn)
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WIFI] Disconnected. Reconnecting...");
        WiFi.reconnect();
        delay(2000);
        return;
    }

    // Giữ kết nối MQTT, retry không blocking (cách mỗi 5s)
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastMqttReconnectAttempt > 5000) {
            lastMqttReconnectAttempt = now;
            reconnectMqtt();
        }
    } else {
        mqttClient.loop();
    }

    // Đọc RFID mỗi vòng loop (non-blocking)
    checkRFID();

    // Xử lý lệnh capture NGOÀI callback (tránh block MQTT)
    // Trigger có thể đến từ RFID (checkRFID) hoặc MQTT (mqttCallback)
    if (captureRequested) {
        captureRequested = false;
        processCapture(pendingUid);
    }
}
