#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <math.h>

// Wifi and Line
const char* ssid = "";
const char* password = "";

String lineToken = "";
String groupId   = "";

// Pins
const int buttonPin = 34;
const int buzzerPin = 4;
const int trigPin = 5;
const int echoPin = 18;

// Button variable
bool lastButtonState = LOW;
unsigned long lastButtonPress = 0;
const unsigned long buttonDebounce = 50;
const unsigned long buttonCooldown = 10000;

// MPU6050 variables
const int MPU_ADDR = 0x68;
float accelThreshold = 1.5;
unsigned long lastFallAlert = 0;
const unsigned long fallCooldown = 10000;

// GPS
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
bool gpsFixed = false;

// Ultrasonic variables
unsigned long lastBeepLevel1 = 0;
unsigned long lastBeepLevel2 = 0;
unsigned long lastBeepLevel3 = 0;
unsigned long lastBeepLevel4 = 0;
unsigned long lastDistanceCheck = 0;
unsigned long lastOutOfRangeBeep = 0;
const unsigned long distanceInterval = 500;

// Offline buffer
#define MAX_OFFLINE_MSG 10
String offlineMessages[MAX_OFFLINE_MSG];
int offlineCount = 0;
bool wasOffline = true;

// Util
String escapeJson(String str) {
  str.replace("\\", "\\\\");
  str.replace("\"", "\\\"");
  str.replace("\n", "\\n");
  str.replace("\r", "");
  return str;
}

// Line
void sendLineMessage(String message) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin("https://api.line.me/v2/bot/message/push");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + lineToken);

  String payload =
    "{\"to\":\"" + groupId +
    "\",\"messages\":[{\"type\":\"text\",\"text\":\"" +
    escapeJson(message) + "\"}]}";

  http.POST(payload);
  http.end();
}

void sendLineMessageSafe(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    sendLineMessage(message);
  } else {
    if (offlineCount < MAX_OFFLINE_MSG) {
      offlineMessages[offlineCount++] = message;
    }
  }
}

void flushOfflineMessages() {
  if (WiFi.status() == WL_CONNECTED && offlineCount > 0) {
    sendLineMessage("Back Online – sending stored alerts");
    for (int i = 0; i < offlineCount; i++) {
      sendLineMessage(offlineMessages[i]);
      delay(300);
    }
    offlineCount = 0;
  }
}

// Alert with GPS
void sendAlertWithGPS(String prefix) {
  String message;
  if (gps.location.isValid()) {
    message = prefix + "\nhttps://maps.google.com/?q=" +
              String(gps.location.lat(), 6) + "," +
              String(gps.location.lng(), 6);
  } else {
    message = prefix + "\nLocation not available";
  }
  sendLineMessageSafe(message);
}

// Buzzer Settings
void beepBuzzer(int freq, int duration) {
  tone(buzzerPin, freq, duration);
}

// MPU6050 Settings
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

// Ultrasonic Settings
float readUltrasonicCM() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000);
  return duration * 0.034 / 2.0;
}

// Setup
void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Start wifi but no need to wait
  WiFi.begin(ssid, password);

  setupMPU();
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);

  // Power on beep
  beepBuzzer(2000, 200);
}

// Loop
void loop() {
  unsigned long currentMillis = millis();

  // Wifi Connect
  static unsigned long lastWifiAttempt = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiAttempt > 5000) {
      WiFi.begin(ssid, password);
      lastWifiAttempt = millis();
    }
  }

  // Wifi state change
  if (WiFi.status() == WL_CONNECTED) {
    if (wasOffline) {
      sendLineMessage("Smart Cane Online!");
      flushOfflineMessages();
      wasOffline = false;
    }
  } else {
    wasOffline = true;
  }

  // GPS
  while (gpsSerial.available()) gps.encode(gpsSerial.read());
  if (gps.location.isValid() && !gpsFixed) {
    gpsFixed = true;
    beepBuzzer(3000, 200);
  }

  // Button
  bool reading = digitalRead(buttonPin);
  if (reading == HIGH && lastButtonState == LOW &&
      (currentMillis - lastButtonPress > buttonDebounce) &&
      (currentMillis - lastButtonPress > buttonCooldown)) {

    beepBuzzer(2500, 150);
    sendAlertWithGPS("Emergency Button Pressed!");
    lastButtonPress = currentMillis;
  }
  lastButtonState = reading;

  // Fall detection
  float totalG = readMPU();
  if (totalG > accelThreshold &&
      currentMillis - lastFallAlert > fallCooldown) {

    lastFallAlert = currentMillis;
    beepBuzzer(1500, 300);
    sendAlertWithGPS("Fall Detected! G=" + String(totalG, 2));
  }

  // Obstacle Detection
  if (millis() - lastDistanceCheck >= distanceInterval) {
    lastDistanceCheck = millis();
    float distanceCM = readUltrasonicCM();

    if (distanceCM <= 0 || distanceCM > 400 || isnan(distanceCM)) {
      if (millis() - lastOutOfRangeBeep > 500) {
        beepBuzzer(2000, 100);
        lastOutOfRangeBeep = millis();
      }
    }
    else if (distanceCM < 25 && millis() - lastBeepLevel4 > 1000) {
      beepBuzzer(3000, 100);
      lastBeepLevel4 = millis();
    }
    else if (distanceCM < 30 && millis() - lastBeepLevel3 > 2000) {
      beepBuzzer(2500, 100);
      lastBeepLevel3 = millis();
    }
    else if (distanceCM < 60){
    }
    else if (millis() - lastBeepLevel1 > 5000) {
      beepBuzzer(1500, 100);
      lastBeepLevel1 = millis();
    }
  }

  delay(200);
}
