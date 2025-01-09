#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>

// Chân điều khiển LED và số lượng LED trên dải NeoPixel
#define PIN D6                              // Chân GPIO nối với dải LED
#define NUMPIXELS 16                        // Tổng số LED trong dải


// Đối tượng NeoPixel để điều khiển LED
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

String status = "OFF";                     // Trạng thái đèn: "ON" hoặc "OFF"
String color = "#000000";                  // Mã màu LED dưới dạng hex
int brightness = 100;                      // Độ sáng LED (0-255)

void setup() {
  // put your setup code here, to run once:
  // Khởi tạo dải LED
  strip.begin();
  strip.show();  // Tắt tất cả LED ban đầu
}

void loop() {
  // put your main code here, to run repeatedly:

}
