#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>

// WiFi thông tin
#define WIFI_SSID "Phong107"                // Tên mạng Wi-Fi
#define WIFI_PASSWORD "hoanghuy1234"        // Mật khẩu Wi-Fi

// Thời gian tối đa để chờ kết nối Wi-Fi (tính bằng mili giây)
const unsigned long WIFI_TIMEOUT = 20000; // 20 giây

// Chân điều khiển LED và số lượng LED trên dải NeoPixel
#define PIN D6                              // Chân GPIO nối với dải LED
#define NUMPIXELS 16                        // Tổng số LED trong dải

// Đối tượng NeoPixel để điều khiển LED
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

bool status = 0;                     // Trạng thái đèn: 1 hoặc 0
String color = "#FFFFFF";                  // Mã màu LED dưới dạng hex
int brightness = 100;                      // Độ sáng LED (0-255)

void setup() {
  Serial.begin(115200);                    // Khởi động Serial Monitor
  // Khởi tạo dải LED
  strip.begin();
  strip.show();  // Tắt tất cả LED ban đầu
  delay(3000);
  // put your setup code here, to run once:
  // Kết nối Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");

  unsigned long startAttemptTime = millis(); // Lấy thời gian bắt đầu
  
  // Lặp để chờ kết nối Wi-Fi
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT) {
    Serial.print(".");
    setAllPixels(hexToColor("#FFD700"));    // Màu vàng
    delay(500);
  }

  // Kiểm tra kết quả sau khi hết thời gian chờ
  if (WiFi.status() == WL_CONNECTED) {
    setAllPixels(hexToColor("#008001"));    // Màu xanh lá cây
    Serial.println("\nWi-Fi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    setAllPixels(hexToColor("#FF0000"));    // Màu đỏ
    Serial.println("\nFailed to connect to Wi-Fi.");
   
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  
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
