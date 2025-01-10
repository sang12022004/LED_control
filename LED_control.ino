#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// WiFi AP
const char* apSSID = "LED16_control";       // Tên mạng Wi-Fi Access Point
const char* apPassword = "12345678";        // Mật khẩu Access Point

// Thời gian tối đa để chờ kết nối Wi-Fi (tính bằng mili giây)
const unsigned long WIFI_TIMEOUT = 20000;  // 20 giây

WiFiUDP udp;                                // Đối tượng UDP
const unsigned int udpPort = 4210;          // Cổng UDP ESP sẽ lắng nghe
char incomingPacket[255];                   // Bộ đệm để lưu dữ liệu nhận được

// Cấu hình WebSocket
String WS_HOST   = "192.168.170.173"; 
uint16_t WS_PORT = 4000;          
String WS_PATH   = "/?deviceId=7"; // Tuỳ ý

// Chân điều khiển LED và số lượng LED trên dải NeoPixel
#define PIN D6                              // Chân GPIO nối với dải LED
#define NUMPIXELS 16                        // Tổng số LED trong dải

WebSocketsClient webSocket;   // Tạo đối tượng WebSocket

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
  handleUDPMessage();  // Xử lý tin nhắn UDP
  if (WiFi.status() == WL_CONNECTED) {
    webSocket.loop();  // Duy trì kết nối WebSocket
  }
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

    if (ssid.isEmpty() || password.isEmpty()) {
      Serial.println("[UDP] Invalid data, missing SSID or PASSWORD");
      return;
    }

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
    strip.clear();
    strip.show();  // Tắt LED
  } else if (status == "CONNECTED") {
    setAllPixels(hexToColor("#008001"));  // Màu xanh lá cây
  } else if (status == "FAILED") {
    setAllPixels(hexToColor("#FF0000"));  // Màu đỏ
  } else if (status == "OFF") {
    strip.clear();  // Tắt tất cả LED
    strip.show();
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

// **10 Callback WebSocket khi có event
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected");
      break;

    case WStype_CONNECTED:
      Serial.println("[WS] Connected to server");
      break;

    case WStype_TEXT: {
      // Server gửi chuỗi JSON
      Serial.printf("[WS] Received: %s\n", payload);

      // Parse JSON
      DynamicJsonDocument doc(256);
      DeserializationError err = deserializeJson(doc, payload);
      
      if (err) {
        Serial.println("[WS] JSON parse error");
        Serial.print("[WS] Error details: ");
        Serial.println(err.c_str());
        return;
      }

      // { "action": "toggle", "powerStatus": true, "color": "#FF0000", "brightness": 80 }
      String action     = doc["action"]     | "";
      bool powerStatus  = doc["powerStatus"] | false;
      String newColor   = doc["color"]      | "#FFFFFF";
      int newBright     = doc["brightness"] | 100;

      if (action == "toggle") {
        ledPower     = powerStatus;
        ledColor     = newColor; 
        ledBrightness= newBright; 
        updateLED();
        Serial.printf("[WS] Toggle LED -> Power=%s, Color=%s, Brightness=%d\n",
                      (ledPower ? "ON" : "OFF"), ledColor.c_str(), ledBrightness);
      }
      else if (action == "updateAttributes") {
        ledPower     = true;
        ledColor     = newColor; 
        ledBrightness= newBright; 
        updateLED();
        Serial.printf("[WS] Toggle LED -> Power=%s, Color=%s, Brightness=%d\n",
                      (ledPower ? "ON" : "OFF"), ledColor.c_str(), ledBrightness);
      }
      else {
        Serial.println("[WS] Unknown action");
      }

    } break;

    default:
      // WStype_BIN, WStype_ERROR, ...
      break;
  }
}


// **11 Khởi tạo WebSocket
void startWebSocket() {
  webSocket.onEvent(webSocketEvent);
  webSocket.begin(WS_HOST.c_str(), WS_PORT, WS_PATH.c_str());
  webSocket.setReconnectInterval(5000);  // Tự động reconnect mỗi 5 giây
  unsigned long startTime = millis();

  while (!webSocket.isConnected() && millis() - startTime < 10000) { // Timeout sau 10 giây
    webSocket.loop();
    delay(100);
  }

  if (webSocket.isConnected()) {
    Serial.println("[WS] WebSocket connected");
  } else {
    Serial.println("[WS] WebSocket connection failed");
  }
}
