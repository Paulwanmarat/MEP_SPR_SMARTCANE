#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <math.h>

// Wifi and line
const char* ssid = "";
const char* password = "";

String lineToken = "";
String groupId   = "";

// Pins
const int buttonPin = 34;
const int buzzerPin = 4;
const int trigPin = 5;
const int echoPin = 18;

// Button settings
bool lastButtonState = LOW;
unsigned long lastButtonPress = 0;
const unsigned long buttonDebounce = 50;
const unsigned long buttonCooldown = 10000; // 10 seconds cooldown

// MPU6050 Settings
const int MPU_ADDR = 0x68;
float accelThreshold = 1.5; // g
unsigned long lastFallAlert = 0;
const unsigned long fallCooldown = 10000; // 10 seconds cooldown

// GPS settings
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
bool gpsFixed = false;     

// Ultrasonic beep timers
unsigned long lastBeepLevel1 = 0;
unsigned long lastBeepLevel2 = 0;
unsigned long lastBeepLevel3 = 0;
unsigned long lastBeepLevel4 = 0;

// Functions
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

// Send alert with GPS link
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

// Buzzer
void beepBuzzer(int freq, int duration) {
  tone(buzzerPin, freq, duration);
}

// MPU setup
void setupMPU() {
  Wire.begin(21, 22);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
}

// Read MPU acceleration
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

// Ultrasonic distance
float readUltrasonicCM() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000); // timeout 30ms
  float distanceCM = duration * 0.034 / 2.0;
  return distanceCM;
}

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Wifi
  Serial.println("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
  sendLineMessage("Smart Cane Online!");

  // MPU
  setupMPU();

  // GPS
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
}

void loop() {
  unsigned long currentMillis = millis();

  // GPS parsing
  while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());

  if (gps.location.isValid() && !gpsFixed) {
    gpsFixed = true;
    Serial.println("GPS FIXED!");
    beepBuzzer(3000, 200);
  }

  if (gps.location.isValid()) {
    Serial.print("Lat: "); Serial.print(gps.location.lat(), 6);
    Serial.print("Lng: "); Serial.print(gps.location.lng(), 6);
    Serial.print("Sats: "); Serial.println(gps.satellites.value());
  }

  // Button press
  bool reading = digitalRead(buttonPin);
  if (reading == HIGH && lastButtonState == LOW && (currentMillis - lastButtonPress > buttonDebounce)) {
    if (currentMillis - lastButtonPress > buttonCooldown) {
      Serial.println("| BUTTON PRESSED");
      beepBuzzer(2500, 150);
      sendAlertWithGPS("Emergency Button Pressed!");
      lastButtonPress = currentMillis;
    } else {
      Serial.println("Press ignored due to cooldown.");
    }
  }
  lastButtonState = reading;

  // Fall detection
  float totalG = readMPU();
  if (totalG > accelThreshold) {
    if (currentMillis - lastFallAlert > fallCooldown) {
      lastFallAlert = currentMillis;
      Serial.println("| FALL DETECTED! G=" + String(totalG,2));
      beepBuzzer(1500, 300);
      sendAlertWithGPS("Fall Detected! G=" + String(totalG,2));
    } else {
      Serial.println("FALL Ignored due to cooldown. G=" + String(totalG,2));
    }
  }

  // 4 Levels
  float distanceCM = readUltrasonicCM();
  Serial.print("Distance: "); Serial.print(distanceCM); Serial.println(" cm");

  if (distanceCM >= 55 && distanceCM <= 75) {
    if (currentMillis - lastBeepLevel1 >= 5000) {
      beepBuzzer(2000, 200);
      lastBeepLevel1 = currentMillis;
      Serial.println("Level 1");
    }
  } else if (distanceCM >= 40 && distanceCM <= 54) { 
    if (currentMillis - lastBeepLevel2 >= 3000) {
      beepBuzzer(2000, 200);
      lastBeepLevel2 = currentMillis;
      Serial.println("Level 2");
    }
  } else if (distanceCM >= 25 && distanceCM <= 39) { 
    if (currentMillis - lastBeepLevel3 >= 2000) {
      beepBuzzer(2000, 200);
      lastBeepLevel3 = currentMillis;
      Serial.println("Level 3");
    }
  } else if (distanceCM < 25 && distanceCM > 0) { 
    if (currentMillis - lastBeepLevel4 >= 1000) {
      beepBuzzer(2000, 200);
      lastBeepLevel4 = currentMillis;
      Serial.println("Level 4");
    }
  }

  delay(200);
}
