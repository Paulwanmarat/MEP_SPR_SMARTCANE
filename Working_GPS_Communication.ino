#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <math.h>

const char* ssid = "";
const char* password = "";

String lineToken = "";
String groupId   = "";

const int buttonPin = 34;
const int buzzerPin = 4;

bool lastButtonState = LOW;
unsigned long lastButtonPress = 0;
const unsigned long buttonDebounce = 50;

const int MPU_ADDR = 0x68;
float accelThreshold = 1.5;
unsigned long lastFallAlert = 0;
const unsigned long fallDelay = 2000;

TinyGPSPlus gps;
HardwareSerial gpsSerial(1); // UART1
bool gpsFixed = false;       // to detect first GPS fix

String escapeJson(String str) {
  str.replace("\\", "\\\\");
  str.replace("\"", "\\\"");
  str.replace("\n", "\\n");
  str.replace("\r", "");
  return str;
}

void sendLineMessage(String message) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin("https://api.line.me/v2/bot/message/push");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + lineToken);

  String payload = "{\"to\":\"" + groupId + "\",\"messages\":[{\"type\":\"text\",\"text\":\"" + escapeJson(message) + "\"}]}";
  int code = http.POST(payload);

  if (code == 200) Serial.println("[LINE] Message sent!");
  else Serial.println("[LINE] Response code: " + String(code));

  http.end();
}

void sendAlertWithGPS(String prefix) {
  String message;
  float lat, lng;

  if (gps.location.isValid()) {
    lat = gps.location.lat();
    lng = gps.location.lng();
    message = prefix + "\nhttps://maps.google.com/?q=" + String(lat,6) + "," + String(lng,6);
  } else {
    lat = 13.7563;
    lng = 100.5018;
    message = prefix + "\nLocation not fixed. Fallback: https://maps.google.com/?q=" + String(lat,6) + "," + String(lng,6);
  }

  sendLineMessage(message);
}

void beepBuzzer(int freq, int duration) {
  tone(buzzerPin, freq, duration);
}

void setupMPU() {
  Wire.begin(21, 22); 
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
}

float readMPU() {
  int16_t AcX, AcY, AcZ;
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  AcX = Wire.read() << 8 | Wire.read();
  AcY = Wire.read() << 8 | Wire.read();
  AcZ = Wire.read() << 8 | Wire.read();

  float gX = AcX / 16384.0;
  float gY = AcY / 16384.0;
  float gZ = AcZ / 16384.0;
  return sqrt(gX*gX + gY*gY + gZ*gZ);
}

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT);
  pinMode(buzzerPin, OUTPUT);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
  sendLineMessage("Smart Cane Online!");

  setupMPU();

  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("---------------------------");
  Serial.println("| BUTTON | FALL | ACCEL(g)|");
  Serial.println("---------------------------");
}

void loop() {
  unsigned long currentMillis = millis();

  while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());

  if (gps.location.isValid() && !gpsFixed) {
    gpsFixed = true;
    Serial.println("| GPS FIXED!");
    beepBuzzer(3000, 200);
  }

  if (gps.location.isValid()) {
    Serial.print("Lat: "); Serial.print(gps.location.lat(), 6);
    Serial.print(" | Lng: "); Serial.print(gps.location.lng(), 6);
    Serial.print(" | Sats: "); Serial.println(gps.satellites.value());
  }

  // Button press
  bool reading = digitalRead(buttonPin);
  if (reading == HIGH && lastButtonState == LOW && (currentMillis - lastButtonPress > buttonDebounce)) {
    lastButtonPress = currentMillis;
    Serial.println("| BUTTON PRESSED");
    beepBuzzer(2500, 150);
    sendAlertWithGPS("Emergency Button Pressed!");
  }
  lastButtonState = reading;

  float totalG = readMPU();
  if (totalG > accelThreshold && (currentMillis - lastFallAlert > fallDelay)) {
    lastFallAlert = currentMillis;
    Serial.println("| FALL DETECTED! G=" + String(totalG,2));
    beepBuzzer(1500, 300);
    sendAlertWithGPS("Fall Detected! G=" + String(totalG,2));
  }

  delay(200);
}
