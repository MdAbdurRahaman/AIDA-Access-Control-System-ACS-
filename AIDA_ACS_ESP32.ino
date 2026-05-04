#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

// ================= WIFI =================
const char* ssid     = "id";
const char* password = "pass";

// Static IP
IPAddress local_IP(10, 81, 100, 72);
IPAddress gateway(10, 81, 100, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

// ================= SERVER =================
WebServer server(80);

// ================= IMAGE URL =================
const char* IMG_PREFIX =
"http://-----------------/api/v1/health/resize?imageUrl=";
const char* IMG_SUFFIX = "&w=200&h=200";

// ================= SPIFFS PATHS =================
#define BG_ON     "/images/bg.jpg"
#define BG_IDLE   "/images/bg3.jpg"
#define EMP_PATH  "/emp.jpg"

// ================= PINS =================
#define RELAY_PIN   22
#define GREEN_LED   17

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

// ================= DOWNLOAD EMPLOYEE IMAGE =================
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

// ================= BACKGROUND =================
void drawBackground(const char* path) {

  if (!SPIFFS.exists(path)) return;

  tft.setRotation(1);
  TJpgDec.drawFsJpg(0, 0, path);
}

// ================= EMPLOYEE IMAGE =================
void drawEmployeeCenter() {

  if (!SPIFFS.exists(EMP_PATH)) return;

  tft.setRotation(0);

  int x = (tft.width() - 200) / 2;
  int y = (tft.height() - 200) / 2;

  TJpgDec.drawFsJpg(x, y, EMP_PATH);

  tft.setRotation(1);
}

// ================= RELAY LOGIC (INVERTED) =================
// HIGH = CLOSE DOOR
// LOW  = OPEN DOOR

void openDoor() {
  digitalWrite(RELAY_PIN, LOW);   // OPEN
  digitalWrite(GREEN_LED, HIGH);
}

void closeDoor() {
  digitalWrite(RELAY_PIN, HIGH);  // CLOSE
  digitalWrite(GREEN_LED, LOW);
}

// ================= IDLE MODE =================
void enterIdle() {

  drawBackground(BG_IDLE);

  closeDoor();   // SAFE STATE = CLOSE

  activeMode = false;
}

// ================= /ON HANDLER =================
void handleOn() {

  if (!server.hasArg("empPicUrl")) {
    server.send(400, "text/plain", "Missing empPicUrl");
    return;
  }

  String empPic = server.arg("empPicUrl");
  String fullURL = String(IMG_PREFIX) + empPic + IMG_SUFFIX;

  drawBackground(BG_ON);

  if (downloadEmployee(fullURL)) {
    drawEmployeeCenter();
  }

  openDoor();   // 🚪 OPEN (LOW)

  actionStart = millis();
  activeMode = true;

  server.send(200, "text/plain", "DOOR OPENED");
}

// ================= /SILENT HANDLER =================
void handleSilent() {

  drawBackground(BG_IDLE);

  openDoor();   // 🔓 Open (Low)

  actionStart = millis();
  activeMode = true;

  server.send(200, "text/plain", "DOOR CLOSED");
}

// ================= SPIFFS DEBUG =================
void listSPIFFS() {

  Serial.println("\n=== SPIFFS FILE LIST ===");

  File root = SPIFFS.open("/");
  File file = root.openNextFile();

  while (file) {
    Serial.print("FILE: ");
    Serial.println(file.name());
    file = root.openNextFile();
  }

  Serial.println("=== END ===\n");
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  // SAFE START → DOOR CLOSED
  closeDoor();

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS ERROR");
    return;
  }

  listSPIFFS();

  WiFi.config(local_IP, gateway, subnet, dns);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
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