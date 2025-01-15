/**************************************************************
 * ESP8266 + NeoPixel + WebSocket Demo
 * 
 * Chỉ dùng WS, không dùng Firebase. 
 * 
 * Tính năng:
 *  - Kết nối Wi-Fi (SSID, PASS cứng)
 *  - Kết nối WebSocket (ws / wss) đến server Node.js
 *  - Nhận JSON điều khiển LED NeoPixel:
 *    {
 *      "action": "toggle",
 *      "powerStatus": true,
 *      "color": "#FF0000",
 *      "brightness": 100
 *    }
 * 
 * Thư viện cần cài:
 *  - arduinoWebSockets
 *  - Adafruit NeoPixel
 **************************************************************/
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>


// Quản lý phiên bản và reset EFROM khi nạo code mới
#define EEPROM_VERSION_ADDR 167 
#define CURRENT_VERSION 2  

// -------- 1) Cấu hình Wi-Fi --------
// const char* WIFI_SSID = "Tuan";
// const char* WIFI_PASSWORD = "Tuan21032001";


// Định nghĩa các giá trị EEPROM lưu trữ SSID và PASS
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 64
#define SSID_MAX_LENGTH 32
#define PASS_MAX_LENGTH 64


// Đọc thông tin WiFi EEPROM
void readWiFiCredentials(String &ssid, String &password) {
  for (int i = 0; i < SSID_MAX_LENGTH; i++) {
    char c = EEPROM.read(SSID_ADDR + i);
    if (c == '\0') break;
    ssid += c;
  }
  
  for (int i = 0; i < PASS_MAX_LENGTH; i++) {
    char c = EEPROM.read(PASS_ADDR + i);
    if (c == '\0') break;
    password += c;
  }
}

// Ghi thông tin WiFi EEPROM
void saveWiFiCredentials(const String &ssid, const String &password) {
  // Xóa dữ liệu cũ
  for (int i = 0; i < SSID_MAX_LENGTH; i++) {
    EEPROM.write(SSID_ADDR + i, 0);
  }
  for (int i = 0; i < PASS_MAX_LENGTH; i++) {
    EEPROM.write(PASS_ADDR + i, 0);
  }

//Ghi mới  
  for (int i = 0; i < ssid.length(); i++) {
    EEPROM.write(SSID_ADDR + i, ssid[i]);
  }
  for (int i = 0; i < password.length(); i++) {
    EEPROM.write(PASS_ADDR + i, password[i]);
  }
  
  EEPROM.commit();
}

// Định nghĩa các giá trị liên quan tới thiết bị
#define LED_COLOR_ADDR 128
#define LED_BRIGHTNESS_ADDR 164 
#define LED_POWER_ADDR 168

// -------- Biến trạng thái --------
bool ledPower = false;        // Bật/tắt
String ledColor = "#FF0000";  // Màu mặc định
int ledBrightness = 125;      // Độ sáng: 0 - 255 (hoặc tuỳ ý)

// Đọc trạng thái đèn từ ROM
void readLEDState() {
  if (EEPROM.read(EEPROM_VERSION_ADDR) != CURRENT_VERSION) {
    resetEEPROM();
    return;
  }
  
  String color = "";
  for (int i = 0; i < 7; i++) {
    char c = EEPROM.read(LED_COLOR_ADDR + i);
    if (c != 0) color += c;
  }
  ledColor = color.length() == 7 ? color : "#FF0000";
  
  ledBrightness = EEPROM.read(LED_BRIGHTNESS_ADDR);
  ledPower = EEPROM.read(LED_POWER_ADDR) == 1;
}

// Lưu trạng thái đèn
void saveLEDState() {
  // Save color
  for (int i = 0; i < 7; i++) {
    EEPROM.write(LED_COLOR_ADDR + i, ledColor[i]);
  }
  
  EEPROM.write(LED_BRIGHTNESS_ADDR, ledBrightness);
  EEPROM.write(LED_POWER_ADDR, ledPower ? 1 : 0);
  EEPROM.commit();
}

//Reset ROM
void resetEEPROM() {
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.write(EEPROM_VERSION_ADDR, CURRENT_VERSION);
  EEPROM.commit();
  
  ledColor = "#FFFFFF";
  ledBrightness = 100;
  ledPower = false;
  saveLEDState();
}


// -------- 2) Cấu hình WebSocket --------
//  - Nếu server Node.js chạy trong LAN, dùng IP laptop, cổng 4000
//  - Nếu dùng ngrok, thay domain + port = 80 (hoặc 443 nếu SSL)
String WS_HOST = "homeconnect-api-ws-3a22df0bbfef.herokuapp.com";
uint16_t WS_PORT = 443;
String deviceId = "5";
String WS_PATH = "/?deviceId=" + deviceId;  // Tuỳ ý

// -------- 3) Cấu hình dải NeoPixel --------
#define LED_PIN D6    // Chân nối dải LED, ví dụ D6 (GPIO12)
#define NUMPIXELS 16  // Số LED NeoPixel
Adafruit_NeoPixel strip(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);


// Đối tượng WebSocket
WebSocketsClient webSocket;

// Biến để quản lý ping định kỳ
#define PING_INTERVAL 25000  // 25 seconds (to match server's ping interval)
#define PONG_TIMEOUT 35000   // 35 seconds (longer than ping interval)
unsigned long lastPingTime = 0;
const unsigned long pingInterval = 15000;  // 15 giây
const unsigned long PING_TIMEOUT = 5000;   // 5 giây
unsigned long lastPongTime = 0;

// Cấu hình Wi-Fi Access Point
const char* apSSID = "LED16";         // Tên Access Point
const char* apPassword = "12345678";  // Mật khẩu Access Point

// Thời gian timeout
const unsigned long WIFI_TIMEOUT = 20000;  // Thời gian chờ kết nối Wi-Fi (20 giây)

/**************************************************************
 * Hàm tạo Access Point
 **************************************************************/
void setupAccessPoint() {
  WiFi.softAP(apSSID, apPassword);  // Tạo Access Point với SSID và mật khẩu
  Serial.println("[AP] Access Point created:");
  Serial.print("SSID: ");
  Serial.println(apSSID);
  Serial.print("Password: ");
  Serial.println(apPassword);
}


// UDP
WiFiUDP udp;                        // Đối tượng UDP để nhận dữ liệu
const unsigned int udpPort = 4210;  // Cổng UDP để lắng nghe
char incomingPacket[255];           // Bộ đệm lưu dữ liệu UDP
WiFiClientSecure client;


// Thiết lập UDP
void setupUDP() {
  udp.begin(udpPort);  // Bắt đầu lắng nghe trên cổng UDP
  Serial.println("[UDP] Listening on port " + String(udpPort));
}

// Xử lý gói tin UDP
void handleUDPMessage() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = '\0';
      String receivedData = String(incomingPacket);
      String ssid = parseValue(receivedData, "SSID");
      String password = parseValue(receivedData, "PASSWORD");
      if (!ssid.isEmpty() && !password.isEmpty()) {
        if (connectToWiFi(ssid, password)) {
          startWebSocket();
        }
      }
    }
  }
}

// Trạng thái LED
void handleLEDStatus(String status) {
  if (status == "CONNECTING") {
    setAllPixels(hexToColor("#FFD700"));  // Hiển thị màu vàng khi đang kết nối
  } else if (status == "CONNECTED") {
    setAllPixels(hexToColor("#008001"));  // Hiển thị màu xanh khi kết nối thành công
    delay(3000);
    strip.clear();  // Tắt toàn bộ LED
    strip.show();
  } else if (status == "FAILED") {
    setAllPixels(hexToColor("#FF0000"));  // Hiển thị màu đỏ khi kết nối thất bại
    delay(3000);
    strip.clear();  // Tắt toàn bộ LED
    strip.show();
  } else {
    strip.clear();  // Tắt toàn bộ LED
    strip.show();
  }
}



// Hàm hỗ trợ
String parseValue(String data, String key) {
  int startIndex = data.indexOf(key + "=");
  if (startIndex == -1) return "";  // Trả về chuỗi rỗng nếu không tìm thấy key
  startIndex += key.length() + 1;
  int endIndex = data.indexOf(";", startIndex);
  return endIndex == -1 ? "" : data.substring(startIndex, endIndex);  // Trích xuất giá trị từ chuỗi
}

uint32_t hexToColor(String hex) {
  if (hex.charAt(0) != '#' || hex.length() != 7) return strip.Color(0, 0, 0);      // Kiểm tra mã màu hợp lệ
  long number = strtol(&hex[1], NULL, 16);                                         // Chuyển mã hex thành số
  return strip.Color((number >> 16) & 0xFF, (number >> 8) & 0xFF, number & 0xFF);  // Trích xuất RGB
}

void setAllPixels(uint32_t color) {
  for (int i = 0; i < NUMPIXELS; i++) {
    strip.setPixelColor(i, color);  // Đặt màu cho từng LED
  }
  strip.show();  // Hiển thị màu sắc trên LED
}

/**************************************************************
 * Hàm cập nhật dải LED
 **************************************************************/
void updateLED() {

  // Set brightness
  // Adafruit_NeoPixel.setBrightness(0 - 255)
  int brightnessScaled = map(ledBrightness, 0, 100, 0, 255);
  // Nếu bạn muốn 100 là max, thay map. Hoặc để raw: strip.setBrightness(ledBrightness).
  strip.setBrightness(brightnessScaled);

  // Nếu power = false => tắt
  if (!ledPower) {
    strip.clear();
    strip.show();
    return;
  }

  // Nếu power = true => set màu toàn bộ
  setAllPixels(hexToColor(ledColor));  // Hiển thị màu
  strip.show();
}

/**************************************************************
 * Callback WebSocket khi có event
 **************************************************************/
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected");
      break;

    case WStype_CONNECTED:
      Serial.println("[WS] Connected to server");
      lastPongTime = millis(); 
      break;

     case WStype_PING:
      lastPongTime = millis();
      break;
      
    case WStype_PONG:
      lastPongTime = millis(); 
      break;

    case WStype_TEXT:
      {
        // Server gửi chuỗi JSON
        Serial.printf("[WS] Received: %s\n", payload);

        // Parse JSON
        DynamicJsonDocument doc(256);
        DeserializationError err = deserializeJson(doc, payload);
        if (err) {
          Serial.println("[WS] JSON parse error");
          return;
        }

        // { "action": "toggle", "powerStatus": true, "color": "#FF0000", "brightness": 80 }
        String action = doc["action"] | "";
        bool powerStatus = doc["powerStatus"] | false;
        String newColor = doc["color"] | "#FFFFFF";
        int newBright = doc["brightness"] | 100;

      

        if (action == "toggle") {
          ledPower = powerStatus;
          ledColor = newColor;
          ledBrightness = newBright;
          updateLED();
          saveLEDState();
          Serial.printf("[WS] Toggle LED -> Power=%s, Color=%s, Brightness=%d\n",
                        (ledPower ? "ON" : "OFF"), ledColor.c_str(), ledBrightness);
        } else if (action == "updateAttributes") {
          ledPower = true;
          ledColor = newColor;
          ledBrightness = newBright;
         updateLED();
          saveLEDState();
          Serial.printf("[WS] Toggle LED -> Power=%s, Color=%s, Brightness=%d\n",
                        (ledPower ? "ON" : "OFF"), ledColor.c_str(), ledBrightness);
        } else {
          Serial.println("[WS] Unknown action");
        }
      }
      break;

    default:
      // WStype_BIN, WStype_ERROR, ...
      break;
  }
}

/**************************************************************
 * Kết nối Wi-Fi
 **************************************************************/
bool connectToWiFi(const String &ssid, const String &password) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  }
  
  handleLEDStatus("CONNECTING");
  Serial.println("[WiFi] Connecting to: " + ssid);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime > WIFI_TIMEOUT) {
      handleLEDStatus("FAILED");
      Serial.println("[WiFi] Connection failed");
      return false;
    }
    delay(500);
  }
  
  Serial.println("[WiFi] Connected successfully");
  handleLEDStatus("CONNECTED");
  saveWiFiCredentials(ssid, password); // Add this line
  return true;
}

/**************************************************************
 * Khởi tạo WebSocket
 **************************************************************/
void startWebSocket() {
  // Gắn callback
  webSocket.onEvent(webSocketEvent);

  // Kết nối ws:
  webSocket.beginSSL(WS_HOST.c_str(), WS_PORT, WS_PATH.c_str());
  webSocket.enableHeartbeat(10000, 2000, 2);  // Giảm thời gian heartbeat
  webSocket.setReconnectInterval(5000);       // Try reconnect every 5 seconds

  unsigned long startTime = millis();
  while (!webSocket.isConnected() && millis() - startTime < 10000) {  // Thử kết nối trong 10 giây
    webSocket.loop();
    Serial.println("[WS] Attempting to connect...");
    handleLEDStatus("CONNECTING");
    delay(500);
  }

  if (webSocket.isConnected()) {
    Serial.println("[WS] WebSocket connected successfully!");
    handleLEDStatus("CONNECTED");
  } else {
    Serial.println("[WS] WebSocket connection failed!");
    handleLEDStatus("FAILED");
  }
}

/**************************************************************
 * SETUP
 **************************************************************/
void setup() {
  Serial.begin(115200);
 EEPROM.begin(EEPROM_SIZE);
  readLEDState();
  
 //Khởi tạo LED
  strip.begin();
  strip.show();
  strip.clear();
  
  // Đọc dữ liệu từ EFROM
  String storedSSID = "", storedPassword = "";
  readWiFiCredentials(storedSSID, storedPassword);
  
  // Thử kết nối wifi
  if (!storedSSID.isEmpty() && !storedPassword.isEmpty()) {
    if (connectToWiFi(storedSSID, storedPassword)) {
      startWebSocket();
      return;
    }
  }
  
  // Nếu thất bại thì khởi tạo Access Point
  setupAccessPoint();
  setupUDP();

}

/**************************************************************
 * LOOP
 **************************************************************/
void loop() {
  handleUDPMessage();
  
  if (WiFi.status() == WL_CONNECTED) {
    if (webSocket.isConnected()) {
      webSocket.loop();

      // Check for pong timeout
      if (millis() - lastPongTime > PONG_TIMEOUT) {
        Serial.println("[WS] No pong received - disconnecting");
        webSocket.disconnect();
        lastPongTime = millis(); 
        delay(1000);
        startWebSocket();
        return;
      }
    } else {
      startWebSocket();
      delay(5000);
    }
  }
}
