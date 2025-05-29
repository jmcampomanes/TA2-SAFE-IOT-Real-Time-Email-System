#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <time.h>  // ⏱ Time library for NTP

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// WiFi Credentials
#define WIFI_SSID "CHAEYOUNG"
#define WIFI_PASSWORD "12345678"

// Firebase Credentials
#define API_KEY "AIzaSyDz6w52CBVze54koDqkftq0O0XhTRJrDbE"
#define DATABASE_URL "https://safe-iot-cc63a-default-rtdb.firebaseio.com"

// Firebase Auth (email/password)
#define USER_EMAIL "lylajanevillanueva@gmail.com"
#define USER_PASSWORD "Putangina120404" // ⚠️ Secure this in production

// Sensor and Output Pins
const int gasPin = 34;
const int airPin = 35;
const int relayPin = 32;
const int redLEDPin = 5;
const int yellowLEDPin = 2;
const int greenLEDPin = 4;

// Thresholds
const int GAS_HIGH = 1000;
const int GAS_WARN = 800;
const int AIR_CLEAN = 750;
const int AIR_POLLUTED = 1000;

// State variable to track alert status
bool alertActive = false;

// Format the current date and time
String getFormattedTime() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

// Format timestamp for Firebase path (can't have colons or spaces)
String getPathSafeTime() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &timeinfo);
  return String(buffer);
}

// Function to write data to Firebase
void writeToFirebase(int gasValue, String gasStatus, int airValue, String airStatus) {
  String timestampStr = getFormattedTime();
  String safePathTime = getPathSafeTime();
  String path = "/logs/" + safePathTime;

  FirebaseJson json;
  json.set("gas", gasValue);
  json.set("gasStatus", gasStatus);
  json.set("airQuality", airValue);
  json.set("airStatus", airStatus);
  json.set("timestamp", millis());
  json.set("datetime", timestampStr);

  if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json)) {
    Serial.println("RTDB write success.");
  } else {
    Serial.printf("RTDB write failed: %s\n", fbdo.errorReason().c_str());
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(relayPin, OUTPUT);
  pinMode(redLEDPin, OUTPUT);
  pinMode(yellowLEDPin, OUTPUT);
  pinMode(greenLEDPin, OUTPUT);

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Sync time with NTP for Philippine Time (UTC+8, no DST)
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for time sync");
  while (time(nullptr) < 100000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nTime synchronized!");

  // Firebase configuration
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  int gasValue = analogRead(gasPin);
  int airValue = analogRead(airPin);

  // Determine air quality status
  String airStatus = (airValue < AIR_CLEAN) ? "Clean" :
                     (airValue < AIR_POLLUTED) ? "Moderate" : "Polluted";

  // Determine gas status
  String gasStatus;
  if (gasValue >= GAS_HIGH) {
    gasStatus = "Danger";
  } else if (gasValue >= GAS_WARN) {
    gasStatus = "Warning";
  } else {
    gasStatus = "Safe";
  }

  // Reset LEDs
  digitalWrite(redLEDPin, LOW);
  digitalWrite(yellowLEDPin, LOW);
  digitalWrite(greenLEDPin, LOW);

  // Determine LED and relay output
  bool gasHigh = gasValue >= GAS_HIGH;
  bool gasWarn = gasValue >= GAS_WARN && gasValue < GAS_HIGH;
  bool airPolluted = airValue >= AIR_POLLUTED;
  bool airWarn = airValue >= AIR_CLEAN && airValue < AIR_POLLUTED;

  if (gasHigh || airPolluted) {
    blinkLED(redLEDPin, 12);
  } else if (gasWarn || airWarn) {
    blinkLED(yellowLEDPin, 12);
  } else {
    digitalWrite(greenLEDPin, HIGH);
  }

  digitalWrite(relayPin, (gasHigh || airPolluted) ? HIGH : LOW);

  Serial.printf("Air: %d (%s) | Gas: %d (%s)\n", airValue, airStatus.c_str(), gasValue, gasStatus.c_str());

  // Check if we are in alert condition (Moderate/Danger or Warning/Danger)
  bool isAlertStatus = (gasStatus == "Warning" || gasStatus == "Danger") ||
                       (airStatus == "Moderate" || airStatus == "Polluted");

  if (isAlertStatus && !alertActive) {
    // Enter alert state, write alert status once
    writeToFirebase(gasValue, gasStatus, airValue, airStatus);
    alertActive = true;
  } else if (!isAlertStatus && alertActive) {
    // First clean/safe after alert, write once and reset alert state
    writeToFirebase(gasValue, gasStatus, airValue, airStatus);
    alertActive = false;
  }
  // Else no write if no change in alert state

  delay(2000);
}

void blinkLED(int pin, int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(200);
    digitalWrite(pin, LOW);
    delay(200);
  }
}