#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

// WiFi AP
const char* apSSID = "LED16_control";       // Tên mạng Wi-Fi Access Point
const char* apPassword = "12345678";        // Mật khẩu Access Point

// Thời gian tối đa để chờ kết nối Wi-Fi (tính bằng mili giây)
const unsigned long WIFI_TIMEOUT = 20000;  // 20 giây

WiFiUDP udp;                                // Đối tượng UDP
const unsigned int udpPort = 4210;          // Cổng UDP ESP sẽ lắng nghe
char incomingPacket[255];                   // Bộ đệm để lưu dữ liệu nhận được

// Chân điều khiển LED và số lượng LED trên dải NeoPixel
#define PIN D6                              // Chân GPIO nối với dải LED
#define NUMPIXELS 16                        // Tổng số LED trong dải

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  delay(3000);

  // Khởi tạo dải LED
  strip.begin();
  strip.show();  // Tắt tất cả LED ban đầu

  // Khởi tạo Access Point
  setupAccessPoint();

  // Khởi tạo UDP
  setupUDP();
}

void loop() {
  handleUDPMessage();
  
}

// **1. Khởi tạo Access Point**
void setupAccessPoint() {
  WiFi.softAP(apSSID, apPassword);
  IPAddress ip = WiFi.softAPIP();
  Serial.println("Access Point đã được tạo:");
  Serial.print("SSID: ");
  Serial.println(apSSID);
  Serial.print("Password: ");
  Serial.println(apPassword);
  Serial.print("IP Address: ");
  Serial.println(ip);
}

// **2. Khởi tạo UDP**
void setupUDP() {
  udp.begin(udpPort);
  Serial.print("Đang lắng nghe UDP trên cổng: ");
  Serial.println(udpPort);
}

// **3. Xử lý gói tin UDP**
void handleUDPMessage() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    Serial.print("Nhận được gói UDP với kích thước: ");
    Serial.println(packetSize);

    int len = udp.read(incomingPacket, 255);
    if (len > 0) incomingPacket[len] = '\0';

    String receivedData = String(incomingPacket);
    String ssid = parseValue(receivedData, "SSID");
    String password = parseValue(receivedData, "PASSWORD");

    Serial.print("SSID nhận được: ");
    Serial.println(ssid);
    Serial.print("Password nhận được: ");
    Serial.println(password);

    connectToWiFi(ssid, password); // Kết nối tới Wi-Fi
  }
}

// **4. Kết nối Wi-Fi**
void connectToWiFi(String ssid, String password) {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT) {
    Serial.print(".");
    handleLEDStatus("CONNECTING");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    handleLEDStatus("CONNECTED");
    delay(3000);
  } else {
    Serial.println("\nFailed to connect to Wi-Fi.");
    handleLEDStatus("FAILED");
    delay(3000);
  }
  handleLEDStatus("OFF");
}

// **5. Hiển thị trạng thái LED**
void handleLEDStatus(String status) {
  if (status == "CONNECTING") {
    setAllPixels(hexToColor("#FFD700"));  // Màu vàng
    delay(500);
    setAllPixels(hexToColor("#000000"));  // Tắt LED
  } else if (status == "CONNECTED") {
    setAllPixels(hexToColor("#008001"));  // Màu xanh lá cây
  } else if (status == "FAILED") {
    setAllPixels(hexToColor("#FF0000"));  // Màu đỏ
  } else if (status == "OFF") {
    setAllPixels(hexToColor("#000000"));  // Tắt tất cả LED
  }
}

// **6. Phân tích chuỗi UDP**
String parseValue(String data, String key) {
  int startIndex = data.indexOf(key + "=");
  if (startIndex == -1) return "";
  startIndex += key.length() + 1;
  int endIndex = data.indexOf(";", startIndex);
  if (endIndex == -1) return "";
  return data.substring(startIndex, endIndex);
}

// **7. Chuyển mã màu hex thành giá trị RGB**
uint32_t hexToColor(String hex) {
  if (hex.charAt(0) != '#' || hex.length() != 7) return strip.Color(0, 0, 0);
  long number = strtol(&hex[1], NULL, 16);
  return strip.Color((number >> 16) & 0xFF, (number >> 8) & 0xFF, number & 0xFF);
}

// **8. Đặt màu cho tất cả LED**
void setAllPixels(uint32_t color) {
  for (int i = 0; i < NUMPIXELS; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

// **9. Hàm cập nhật LED dựa trên trạng thái, màu sắc, và độ sáng**
void updateLED(bool ledStatus, String hexColor, int brightness) {
  if (ledStatus) {                            // Nếu trạng thái là bật
    strip.setBrightness(brightness);          // Cập nhật độ sáng
    setAllPixels(hexToColor(hexColor));       // Cập nhật màu sắc
    Serial.println("LED is ON.");
  } else {                                    // Nếu trạng thái là tắt
    strip.clear();                            // Tắt tất cả LED
    strip.show();
    Serial.println("LED is OFF.");
  }
}
