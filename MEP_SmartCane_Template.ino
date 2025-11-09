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
const unsigned long distanceInterval = 500;
unsigned long lastDistanceCheck = 0;
unsigned long lastOutOfRangeBeep = 0;

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
  http.POST(payload);
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
    message = prefix + "\nLocation not available (GPS not fixed yet)";
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
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
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
    beepBuzzer(3000, 200);
  }

  // Button press
  bool reading = digitalRead(buttonPin);
  if (reading == HIGH && lastButtonState == LOW && (currentMillis - lastButtonPress > buttonDebounce)) {
    if (currentMillis - lastButtonPress > buttonCooldown) {
      beepBuzzer(2500, 150);
      sendAlertWithGPS("Emergency Button Pressed!");
      lastButtonPress = currentMillis;
    }
  }
  lastButtonState = reading;

  // Fall detection
  float totalG = readMPU();
  if (totalG > accelThreshold) {
    if (currentMillis - lastFallAlert > fallCooldown) {
      lastFallAlert = currentMillis;
      beepBuzzer(1500, 300);
      sendAlertWithGPS("Fall Detected! G=" + String(totalG,2));
    }
  }

  // Ultrasonic distance check
  if (millis() - lastDistanceCheck >= distanceInterval) {
    lastDistanceCheck = millis();
    float distanceCM = readUltrasonicCM();

    // Out of range
    if (distanceCM <= 0 || distanceCM > 400 || isnan(distanceCM)) {
        if (millis() - lastOutOfRangeBeep >= 500) {
            beepBuzzer(2000, 100);
            lastOutOfRangeBeep = millis();
        }
    }

    else if (distanceCM < 25) {
        if (millis() - lastBeepLevel4 >= 1000) {
            beepBuzzer(3000, 100);
            lastBeepLevel4 = millis();
            lastBeepLevel1 = lastBeepLevel2 = lastBeepLevel3 = millis();
        }
    }
    else if (distanceCM >= 25 && distanceCM < 32) {
        if (millis() - lastBeepLevel3 >= 2000) {
            beepBuzzer(2500, 100);
            lastBeepLevel3 = millis();
            lastBeepLevel1 = lastBeepLevel2 = lastBeepLevel4 = millis();
        }
    }
    else if (distanceCM >= 32 && distanceCM < 38) {
        if (millis() - lastBeepLevel2 >= 3000) {
            beepBuzzer(2000, 100);
            lastBeepLevel2 = millis();
            lastBeepLevel1 = lastBeepLevel3 = lastBeepLevel4 = millis();
        }
    }
    else if (distanceCM > 53) {
        if (millis() - lastBeepLevel1 >= 5000) {
            beepBuzzer(1500, 100);
            lastBeepLevel1 = millis();
            lastBeepLevel2 = lastBeepLevel3 = lastBeepLevel4 = millis();
        }
    }
  }

  delay(200);
}
