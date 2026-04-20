#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

// ================= WIFI =================
const char* ssid = "ID" // Replace this with your wifi name<<<================================ (then add the ;)
const char* password = "Password"; // Replace this with your wifi password <<<================================

// -------- STATIC IP --------
IPAddress local_IP(10, 81, 100, 72);
IPAddress gateway(10, 81, 100, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

// ================= SERVER =================
WebServer server(80);

// ================= URL PARTS =================
const char* IMG_PREFIX =
"http://------------------------------------";
const char* IMG_SUFFIX = "&w=200&h=200";

// ================= PATHS =================
#define BG_ON     "/images/bg.jpg"
#define BG_IDLE   "/images/bg3.jpg"
#define EMP_PATH  "/emp.jpg"

// ================= PINS =================
#define RELAY_PIN   22
#define GREEN_LED   17
#define SD_CS       5

// ================= DISPLAY =================
TFT_eSPI tft;

// ================= STATE =================
unsigned long actionStart = 0;
bool activeMode = false;

// ================= JPEG CALLBACK =================
bool tft_output(int16_t x, int16_t y,
                uint16_t w, uint16_t h,
                uint16_t* bitmap) {
  if (y >= tft.height()) return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

// ================= DOWNLOAD IMAGE =================
bool downloadEmployee(String url) {
  HTTPClient http;
  WiFiClient client;

  if (!http.begin(client, url)) return false;
  http.addHeader("Accept", "image/jpeg");

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  File file = SPIFFS.open(EMP_PATH, FILE_WRITE);
  if (!file) {
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[1024];
  int len;

  while (http.connected()) {
    len = stream->read(buf, sizeof(buf));
    if (len > 0) file.write(buf, len);
    else break;
  }

  file.close();
  http.end();
  return true;
}

// ================= DRAW BACKGROUND =================
void drawBackground(const char* path) {
  File bg = SD.open(path);
  if (!bg) return;

  int len = bg.size();
  uint8_t* buf = (uint8_t*)malloc(len);
  if (!buf) {
    bg.close();
    return;
  }

  bg.read(buf, len);
  bg.close();

  tft.setRotation(1);
  TJpgDec.drawJpg(0, 0, buf, len);
  free(buf);
}

// ================= DRAW EMPLOYEE =================
void drawEmployeeCenterRotated() {
  if (!SPIFFS.exists(EMP_PATH)) return;

  tft.setRotation(0);
  int x = (tft.width() - 200) / 2;
  int y = (tft.height() - 200) / 2;
  TJpgDec.drawJpg(x, y, EMP_PATH);
  tft.setRotation(1);
}

// ================= IDLE MODE =================
void enterIdle() {
  drawBackground(BG_IDLE);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(GREEN_LED, LOW);
  activeMode = false;
}

// ================= ON HANDLER =================
void handleOn() {
  if (!server.hasArg("empPicUrl")) {
    server.send(400, "text/plain", "Missing empPicUrl");
    return;
  }

  String empPic = server.arg("empPicUrl");
  String fullURL = String(IMG_PREFIX) + empPic + IMG_SUFFIX;

  drawBackground(BG_ON);

  if (downloadEmployee(fullURL)) {
    drawEmployeeCenterRotated();
  }

  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(GREEN_LED, HIGH);

  actionStart = millis();
  activeMode = true;

  server.send(200, "text/plain", "ON OK");
}

// ================= SILENT HANDLER =================
void handleSilent() {
  drawBackground(BG_IDLE);
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(GREEN_LED, HIGH);

  actionStart = millis();
  activeMode = true;

  server.send(200, "text/plain", "SILENT OK");
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  SPIFFS.begin(true);
  SD.begin(SD_CS);

  // -------- APPLY STATIC IP --------
  if (!WiFi.config(local_IP, gateway, subnet, dns)) {
    Serial.println("❌ STA Failed to configure");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  Serial.print("✅ ESP32 IP: ");
  Serial.println(WiFi.localIP());

  server.on("/on", handleOn);
  server.on("/silent", handleSilent);
  server.begin();

  enterIdle();
}

// ================= LOOP =================
void loop() {
  server.handleClient();

  if (activeMode && millis() - actionStart > 5000) {
    enterIdle();
  }
}
