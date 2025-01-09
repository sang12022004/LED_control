#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

//WiFi AP
const char* apSSID = "LED16_control";         // Tên mạng Wi-Fi Access Point
const char* apPassword = "12345678";       // Mật khẩu Access Point

// Thời gian tối đa để chờ kết nối Wi-Fi (tính bằng mili giây)
const unsigned long WIFI_TIMEOUT = 20000; // 20 giây

WiFiUDP udp;                               // Đối tượng UDP
const unsigned int udpPort = 4210;         // Cổng UDP ESP sẽ lắng nghe
char incomingPacket[255];                  // Bộ đệm để lưu dữ liệu nhận được

// Chân điều khiển LED và số lượng LED trên dải NeoPixel
#define PIN D6                              // Chân GPIO nối với dải LED
#define NUMPIXELS 16                        // Tổng số LED trong dải

// Đối tượng NeoPixel để điều khiển LED
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

bool status = 1;                     // Trạng thái đèn: 1 hoặc 0
String color = "#FFFFFF";                  // Mã màu LED dưới dạng hex
int brightness = 100;                      // Độ sáng LED (0-255)

void setup() {
  Serial.begin(115200);                    // Khởi động Serial Monitor
  delay(3000);
  // Khởi tạo dải LED
  strip.begin();
  strip.show();  // Tắt tất cả LED ban đầu
  // 1. Khởi tạo Access Point
  WiFi.softAP(apSSID, apPassword);
  IPAddress ip = WiFi.softAPIP();
  Serial.println("Access Point đã được tạo:");
  Serial.print("SSID: ");
  Serial.println(apSSID);
  Serial.print("Password: ");
  Serial.println(apPassword);
  Serial.print("IP Address: ");
  Serial.println(ip);

  // Khởi tạo UDP
  udp.begin(udpPort);
  Serial.print("Đang lắng nghe UDP trên cổng: ");
  Serial.println(udpPort);
}

void loop() {
  // Tiếp tục xử lý khác trong vòng lặp
  // Nhận gói UDP và phân tích chuỗi
  int packetSize = udp.parsePacket();
  if (packetSize) {
    Serial.print("Nhận được gói UDP với kích thước: ");
    Serial.println(packetSize);

    // Đọc nội dung gói tin
    int len = udp.read(incomingPacket, 255);
    if (len > 0) incomingPacket[len] = '\0';

    String receivedData = String(incomingPacket);
    String ssid = parseValue(receivedData, "SSID");
    String password = parseValue(receivedData, "PASSWORD");

    Serial.print("SSID nhận được: ");
    Serial.println(ssid);
    Serial.print("Password nhận được: ");
    Serial.println(password);

    // 2. Kết nối tới Wi-Fi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi");
  
    unsigned long startAttemptTime = millis(); // Lấy thời gian bắt đầu
    
    // Lặp để chờ kết nối Wi-Fi
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT) {
      Serial.print(".");
      // Chớp nháy đèn vàng
      setAllPixels(hexToColor("#FFD700"));  // Màu vàng
      delay(500);
      setAllPixels(hexToColor("#000000"));  // Tắt LED
      delay(500);
    }
  
    // Kiểm tra kết quả sau khi hết thời gian chờ
    if (WiFi.status() == WL_CONNECTED) {
      setAllPixels(hexToColor("#008001"));    // Màu xanh lá cây
      Serial.println("\nWi-Fi connected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      delay(3000);                            // Giữ đèn xanh trong 3 giây
    } else {
      setAllPixels(hexToColor("#FF0000"));    // Màu đỏ
      Serial.println("\nFailed to connect to Wi-Fi.");
      delay(3000);                            // Giữ đèn đỏ trong 3 giây
    }
  
    // Sau đó tắt đèn để chuyển sang xử lý khác
    setAllPixels(hexToColor("#000000"));      // Tắt tất cả LED
  }
}

// ** Phân tích chuỗi UDP **
String parseValue(String data, String key) {
  int startIndex = data.indexOf(key + "=");
  if (startIndex == -1) return "";
  startIndex += key.length() + 1;
  int endIndex = data.indexOf(";", startIndex);
  if (endIndex == -1) return "";
  return data.substring(startIndex, endIndex);
}

// Hàm cập nhật LED dựa trên các giá trị trạng thái, màu sắc và độ sáng
void updateLED() {
  if (status) {                     // Nếu trạng thái là 1
    strip.setBrightness(brightness);        // Cập nhật độ sáng
    setAllPixels(hexToColor(color));        // Cập nhật màu sắc
    Serial.println("LED is ON.");
  } else {                                  // Nếu trạng thái là 0
    strip.clear();                          // Tắt tất cả LED
    strip.show();
    Serial.println("LED is OFF.");
  }
}

// Hàm chuyển mã màu hex thành giá trị RGB
uint32_t hexToColor(String hex) {
  if (hex.charAt(0) != '#' || hex.length() != 7) return strip.Color(0, 0, 0); // Trả về màu đen nếu mã màu không hợp lệ
  long number = strtol(&hex[1], NULL, 16);  // Chuyển từ chuỗi hex sang số
  return strip.Color((number >> 16) & 0xFF, (number >> 8) & 0xFF, number & 0xFF);
}

// Hàm đặt màu cho tất cả LED trong dải
void setAllPixels(uint32_t color) {
  for (int i = 0; i < NUMPIXELS; i++) {     // Duyệt qua từng LED
    strip.setPixelColor(i, color);          // Đặt màu cho LED
  }
  strip.show();                             // Hiển thị màu sắc mới
}
