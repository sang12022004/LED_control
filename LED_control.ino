#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// Cấu hình Wi-Fi Access Point
const char* apSSID = "LED16_control"; // Tên Access Point
const char* apPassword = "12345678";  // Mật khẩu Access Point

// Thời gian timeout
const unsigned long WIFI_TIMEOUT = 20000; // Thời gian chờ kết nối Wi-Fi (20 giây)

// UDP
WiFiUDP udp;                 // Đối tượng UDP để nhận dữ liệu
const unsigned int udpPort = 4210; // Cổng UDP để lắng nghe
char incomingPacket[255];    // Bộ đệm lưu dữ liệu UDP

// Biến trạng thái LED
int ledPower = 0;            // 0: Tắt LED, 1: Bật LED
String ledColor = "#FF0000"; // Màu mặc định
int ledBrightness = 100;     // Độ sáng LED (0-100)

// WebSocket
String WS_HOST = "192.168.170.173"; // Địa chỉ WebSocket server
uint16_t WS_PORT = 4000;            // Cổng WebSocket
String WS_PATH = "/?deviceId=7";    // Đường dẫn WebSocket
WebSocketsClient webSocket;         // Đối tượng WebSocket

// NeoPixel
#define PIN D6         // Chân GPIO kết nối LED NeoPixel
#define NUMPIXELS 16   // Số lượng LED trên dải NeoPixel
Adafruit_NeoPixel strip(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800); // Khởi tạo dải LED

// SETUP
void setup() {
  Serial.begin(115200);    // Khởi động Serial Monitor
  strip.begin();           // Bắt đầu dải LED
  strip.clear();           // Tắt toàn bộ LED ban đầu
  strip.show();

  setupAccessPoint();      // Tạo Access Point để nhận thông tin Wi-Fi
  setupUDP();              // Bắt đầu lắng nghe dữ liệu UDP
}

// LOOP
void loop() {
  handleUDPMessage();      // Xử lý gói tin UDP nếu có
  if (WiFi.status() == WL_CONNECTED) {
    webSocket.loop();      // Duy trì kết nối WebSocket nếu Wi-Fi đã kết nối
  }
}

// Tạo Access Point
void setupAccessPoint() {
  WiFi.softAP(apSSID, apPassword); // Tạo Access Point với SSID và mật khẩu
  Serial.println("[AP] Access Point created:");
  Serial.print("SSID: ");
  Serial.println(apSSID);
  Serial.print("Password: ");
  Serial.println(apPassword);
}

// Thiết lập UDP
void setupUDP() {
  udp.begin(udpPort); // Bắt đầu lắng nghe trên cổng UDP
  Serial.println("[UDP] Listening on port " + String(udpPort));
}

// Xử lý gói tin UDP
void handleUDPMessage() {
  int packetSize = udp.parsePacket(); // Kiểm tra nếu có gói tin UDP mới
  if (packetSize) {
    int len = udp.read(incomingPacket, 255); // Đọc gói tin vào bộ đệm
    if (len > 0) incomingPacket[len] = '\0'; // Kết thúc chuỗi bằng ký tự NULL

    String receivedData = String(incomingPacket);
    String ssid = parseValue(receivedData, "SSID");      // Lấy giá trị SSID
    String password = parseValue(receivedData, "PASSWORD"); // Lấy giá trị PASSWORD

    if (ssid.isEmpty() || password.isEmpty()) { // Kiểm tra tính hợp lệ
      Serial.println("[UDP] Invalid data, missing SSID or PASSWORD");
      return;
    }

    Serial.println("[UDP] SSID: " + ssid + ", PASSWORD: " + password);
    connectToWiFi(ssid, password); // Kết nối Wi-Fi với SSID và PASSWORD nhận được
  }
}

// Trạng thái LED
void handleLEDStatus(String status) {
  if (status == "CONNECTING") {
    setAllPixels(hexToColor("#FFD700")); // Hiển thị màu vàng khi đang kết nối
  } else if (status == "CONNECTED") {
    setAllPixels(hexToColor("#008001")); // Hiển thị màu xanh khi kết nối thành công
    delay(3000);
    strip.clear(); // Tắt toàn bộ LED
    strip.show();
  } else if (status == "FAILED") {
    setAllPixels(hexToColor("#FF0000")); // Hiển thị màu đỏ khi kết nối thất bại
    delay(3000);
    strip.clear(); // Tắt toàn bộ LED
    strip.show();
  } else {
    strip.clear(); // Tắt toàn bộ LED
    strip.show();
  }
}

// WebSocket
void startWebSocket() {
  webSocket.onEvent(webSocketEvent); // Gắn callback xử lý sự kiện WebSocket
  webSocket.begin(WS_HOST.c_str(), WS_PORT, WS_PATH.c_str()); // Kết nối WebSocket
  webSocket.setReconnectInterval(5000); // Tự động kết nối lại sau 5 giây nếu mất kết nối

  // Timeout sau 10 giây nếu không kết nối được
  unsigned long startTime = millis();
  while (!webSocket.isConnected() && millis() - startTime < 10000) {
    webSocket.loop();
    handleLEDStatus("CONNECTING");
  }

  // Kiểm tra kết quả kết nối
  if (webSocket.isConnected()) {
    Serial.println("[WS] WebSocket connected");
    handleLEDStatus("CONNECTED"); // Hiển thị trạng thái "CONNECTED"
  } else {
    Serial.println("[WS] WebSocket connection failed");
    handleLEDStatus("FAILED"); // Hiển thị trạng thái "FAILED"
  }
}

// Xử lý sự kiện WebSocket
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT) { // Xử lý dữ liệu dạng văn bản từ server
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, payload)) {
      Serial.println("[WS] JSON parse error");
      return;
    }

    ledPower = doc["powerStatus"] | 0; // Lấy trạng thái LED (0 hoặc 1)
    ledColor = doc["color"] | "#FFFFFF"; // Lấy mã màu LED
    ledBrightness = doc["brightness"] | 100; // Lấy độ sáng LED

    updateLED(ledPower, ledColor, ledBrightness); // Cập nhật LED với thông tin nhận được
  }
}


// Kết nối Wi-Fi
void connectToWiFi(String ssid, String password) {
  Serial.println("[WiFi] Starting connection...");
  WiFi.begin(ssid.c_str(), password.c_str()); // Bắt đầu kết nối Wi-Fi

  unsigned long startAttemptTime = millis();  // Lấy thời gian bắt đầu kết nối

  // Chờ kết nối Wi-Fi hoặc hết thời gian timeout
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT) {
    handleLEDStatus("CONNECTING"); // Hiển thị trạng thái đang kết nối
  }

  // Kiểm tra kết nối thành công hay thất bại sau vòng lặp
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] Connected! IP: " + WiFi.localIP().toString());
    handleLEDStatus("CONNECTED"); // Hiển thị trạng thái kết nối thành công
    startWebSocket();             // Khởi động WebSocket nếu Wi-Fi kết nối thành công
  } else {
    Serial.println("[WiFi] Failed to connect. Please check SSID/PASSWORD.");
    handleLEDStatus("FAILED");    // Hiển thị trạng thái kết nối thất bại
  }
}

// Hàm hỗ trợ
String parseValue(String data, String key) {
  int startIndex = data.indexOf(key + "=");
  if (startIndex == -1) return ""; // Trả về chuỗi rỗng nếu không tìm thấy key
  startIndex += key.length() + 1;
  int endIndex = data.indexOf(";", startIndex);
  return endIndex == -1 ? "" : data.substring(startIndex, endIndex); // Trích xuất giá trị từ chuỗi
}

uint32_t hexToColor(String hex) {
  if (hex.charAt(0) != '#' || hex.length() != 7) return strip.Color(0, 0, 0); // Kiểm tra mã màu hợp lệ
  long number = strtol(&hex[1], NULL, 16); // Chuyển mã hex thành số
  return strip.Color((number >> 16) & 0xFF, (number >> 8) & 0xFF, number & 0xFF); // Trích xuất RGB
}

void setAllPixels(uint32_t color) {
  for (int i = 0; i < NUMPIXELS; i++) {
    strip.setPixelColor(i, color); // Đặt màu cho từng LED
  }
  strip.show(); // Hiển thị màu sắc trên LED
}

void updateLED(int ledPower, String hexColor, int brightness) {
  strip.setBrightness(map(brightness, 0, 100, 0, 255)); // Cập nhật độ sáng
  if (ledPower == 0) {
    strip.clear(); // Tắt LED nếu ledPower = 0
  } else {
    setAllPixels(hexToColor(hexColor)); // Hiển thị màu sắc nếu ledPower = 1
  }
  strip.show();
}
