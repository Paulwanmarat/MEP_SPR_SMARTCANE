#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <math.h>

// Wifi and LINE 
const char* ssid = "";
const char* password = "";

String lineToken = "";
String groupId   = "";

// Pins 
const int buttonPin = 34;
const int trigPin = 5;
const int echoPin = 18;
const int buzzerPin = 4;

// Button
bool lastButtonState = LOW;
unsigned long lastButtonPress = 0;
const unsigned long buttonDebounce = 50;

// Fall Detection 
const int MPU_ADDR = 0x68;
float accelThreshold = 1.5; // g
unsigned long lastFallAlert = 0;
const unsigned long fallDelay = 2000;

// Obstacle Detection
unsigned long lastObstacleAlert = 0;
const unsigned long obstacleDelay = 1000;
const int maxDistance = 150;  // cm

// GPS 
TinyGPSPlus gps;
HardwareSerial gpsSerial(1); // UART1

// Smart cane Functions
String escapeJson(String str) {
  str.replace("\\", "\\\\");
  str.replace("\"", "\\\"");
  return str;
}

void sendLineMessage(String message) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin("https://api.line.me/v2/bot/message/push");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + lineToken);

  String payload = "{\"to\":\"" + groupId + "\",\"messages\":[{\"type\":\"text\",\"text\":\"" + escapeJson(message) + "\"}]}";

  int code = 0;
  int attempts = 0;
  while (attempts < 3) {
    code = http.POST(payload);
    if (code == 200) break;
    attempts++;
    delay(2000);
  }

  if (code == 200) Serial.println("[LINE] Message sent successfully!");
  else Serial.println("[LINE] Failed after retries. Code: " + String(code));

  http.end();
}

void sendAlertWithGPS(String prefix) {
  String message;
  float lat, lng;

  if (gps.location.isValid()) {
    lat = gps.location.lat();
    lng = gps.location.lng();
    String link = "https://maps.google.com/?q=" + String(lat, 6) + "," + String(lng, 6);
    message = prefix + "\nLocation: " + link;
  } else {
    // Default Fallback coordinates
    lat = 13.7563;
    lng = 100.5018;
    String link = "https://maps.google.com/?q=" + String(lat, 6) + "," + String(lng, 6);
    message = prefix + "\nLocation: Not fixed, fallback: " + link;
  }

  sendLineMessage(message);
}

// Non blocking buzzer
void beepBuzzer(int freq, int duration) {
  tone(buzzerPin, freq);
  unsigned long start = millis();
  while (millis() - start < duration) { }
  noTone(buzzerPin);
}

long readDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000);
  return duration * 0.0343 / 2;
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
  return sqrt(gX * gX + gY * gY + gZ * gZ);
}

// Setup
void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);

  // Wifi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
  sendLineMessage("Smart Cane Online!");

  setupMPU();

  // GPS
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);

  // Print table header
  Serial.println(F("--------------------------------------------------"));
  Serial.println(F("| BUTTON | FALL | ACCEL (g) | OBSTACLE (cm) |"));
  Serial.println(F("--------------------------------------------------"));
}

void loop() {
  unsigned long currentMillis = millis();

  // GPS
  if (gpsSerial.available() > 0) gps.encode(gpsSerial.read());

  // Button
  bool reading = digitalRead(buttonPin);
  if (reading == HIGH && lastButtonState == LOW && (currentMillis - lastButtonPress > buttonDebounce)) {
    lastButtonPress = currentMillis;
    Serial.print("|  PRESSED  ");
    beepBuzzer(2500, 150);
    sendAlertWithGPS("Emergency Button Pressed!");
  } else if (reading == LOW) {
    Serial.print("|    -     ");
  }
  lastButtonState = reading;

  // Fall Detection
  float totalG = readMPU();
  if (totalG > accelThreshold && (currentMillis - lastFallAlert > fallDelay)) {
    lastFallAlert = currentMillis;
    Serial.print("| FALL!  ");
    beepBuzzer(1500, 300);
    sendAlertWithGPS("Fall detected! G=" + String(totalG));
  } else {
    Serial.print("|   -   ");
  }
  Serial.print("| " + String(totalG, 2) + "    ");

  // Obstacle Detection
  long dist = readDistance();
  if (dist <= maxDistance) {
    lastObstacleAlert = currentMillis;
    Serial.print("| " + String(dist) + "       |");
    if (dist <= 150) beepBuzzer(1000, 200);
  } else {
    Serial.print("|   -      |");
  }

  Serial.println();
  delay(200);
}
