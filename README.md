# MEP_SmartCane_Template.ino

## Project Overview

This project is a Mini English Program (MEP) for senior high school students. The purpose of this project is to create a smart walking cane using an ESP32 microcontroller. The smart cane is designed to help elderly or visually impaired people by improving safety while walking.

The system can detect obstacles in front of the user, detect when a fall occurs, and send emergency alert messages with GPS location to caregivers using the LINE application.

---

## Project Objectives

* To design and build a smart walking cane using ESP32
* To help users detect obstacles while walking
* To detect falls and send emergency alerts automatically
* To send the user’s location to caregivers in emergency situations

---

## Project Features

* Emergency button for sending alert messages manually
* Fall detection using MPU6050 accelerometer
* GPS location tracking and Google Maps link
* Obstacle detection using ultrasonic sensor
* Buzzer alerts with different sounds based on distance
* Message storage when WiFi is not available
* Automatic WiFi reconnection

---

## Hardware Components

| No. | Component         | Description                 |
| --- | ----------------- | --------------------------- |
| 1   | ESP32             | Main controller board       |
| 2   | Ultrasonic Sensor | Used to detect obstacles    |
| 3   | MPU6050           | Used to detect falls        |
| 4   | GPS Module        | Used to get location data   |
| 5   | Push Button       | Emergency alert button      |
| 6   | Passive Buzzer    | Used for sound alerts       |
| 7   | Jumper Wires      | Used for connecting devices |

---

## Pin Connections

| Function         | ESP32 Pin |
| ---------------- | --------- |
| Emergency Button | GPIO 34   |
| Buzzer           | GPIO 4    |
| Ultrasonic TRIG  | GPIO 5    |
| Ultrasonic ECHO  | GPIO 18   |
| GPS RX           | GPIO 16   |
| GPS TX           | GPIO 17   |
| MPU6050 SDA      | GPIO 21   |
| MPU6050 SCL      | GPIO 22   |

---

## Software and Libraries

* Arduino IDE
* ESP32 board support
* Libraries used in this project:

  * WiFi.h
  * HTTPClient.h
  * Wire.h
  * TinyGPSPlus
  * math.h

---

## Program Configuration

Before uploading the program, the following information must be added to the code:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

String lineToken = "YOUR_LINE_CHANNEL_ACCESS_TOKEN";
String groupId   = "YOUR_LINE_GROUP_ID";
```

---

## Alert Message System

The smart cane sends alert messages such as:

* Smart Cane Online
* Emergency Button Pressed
* Fall Detected

If GPS data is available, the alert message will include a Google Maps link showing the user’s location.

---

## Obstacle Detection

The ultrasonic sensor measures the distance to objects in front of the cane. The buzzer works as follows:

| Distance (cm)           | Buzzer Sound  |
| ----------------------- | ------------- |
| Less than 25            | High sound    |
| 25 to 29                | Medium sound  |
| 30 to 59                | No sound      |
| 60 or more              | Low sound     |
| Invalid or out of range | Warning sound |

---

## Fall Detection

The MPU6050 sensor is used to measure acceleration. If the total acceleration value is higher than 1.5 G, the system assumes that the user has fallen and sends an alert message. To avoid repeated alerts, a delay of 10 seconds is applied between each alert.

---

## Offline Mode

If WiFi is disconnected, alert messages are stored in memory (up to 10 messages). When WiFi connection is restored, all stored messages will be sent automatically.

---

## How to Use

1. Upload `MEP_SmartCane_Template.ino` to the ESP32 board
2. Turn on the device
3. Wait for WiFi connection and GPS signal
4. Use the smart cane normally

---

## Notes

* GPS accuracy depends on the environment
* LINE Messaging API must allow push messages
* GPIO 34 is input-only and used for the emergency button

---

## Project Information

MEP Smart Cane Project

This project was created by senior high school students as part of the MEP program for educational purposes.

---

## License

This project is for educational use only.
