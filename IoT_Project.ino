#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// ---------------- Wi-Fi & Firebase Configuration ----------------
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define API_KEY ""
#define DATABASE_URL ""
#define USER_EMAIL ""
#define USER_PASSWORD ""

// ---------------- Pin Definitions ----------------
const int lowLevelPin = 12;
const int highLevelPin = 13;
const int pump1Pin = 25;
const int pump2Pin = 26;
const int valve1Pin = 27;
const int valve2Pin = 14;
const int valve3Pin = 33;

// ---------------- Firebase Objects ----------------
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);

  // Setup pins
  pinMode(lowLevelPin, INPUT);
  pinMode(highLevelPin, INPUT);
  pinMode(pump1Pin, OUTPUT);
  pinMode(pump2Pin, OUTPUT);
  pinMode(valve1Pin, OUTPUT);
  pinMode(valve2Pin, OUTPUT);
  pinMode(valve3Pin, OUTPUT);

  digitalWrite(pump1Pin, LOW);
  digitalWrite(pump2Pin, LOW);
  digitalWrite(valve1Pin, LOW);
  digitalWrite(valve2Pin, LOW);
  digitalWrite(valve3Pin, LOW);

  // Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n✅ Connected to Wi-Fi");

  // Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Wait for auth
  while (auth.token.uid == "") {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n✅ Firebase Authenticated");
}

void loop() {
  // ------------ 1. Read Water Level ------------
  bool low = digitalRead(lowLevelPin);
  bool high = digitalRead(highLevelPin);
  String waterLevel = "MID";

  if (high == HIGH) {
    waterLevel = "HIGH";
    digitalWrite(pump1Pin, LOW);
    Serial.println("Water HIGH - Pumps OFF");
  } else if (low == HIGH) {
    waterLevel = "LOW";
    digitalWrite(pump1Pin, HIGH);
    Serial.println("Water LOW - Pumps ON");
  } else {
    waterLevel = "MID";
    digitalWrite(pump1Pin, HIGH);
    Serial.println("Water MID - Pumps ON");
  }

  // ------------ 2. Update Firebase with Water Level and Pump 1 Status ------------
  if (Firebase.ready()) {
    Firebase.RTDB.setString(&fbdo, "/water/level", waterLevel);
    Firebase.RTDB.setString(&fbdo, "/water/pump_status", (digitalRead(pump1Pin) == HIGH ? "ON" : "OFF"));
  }

  // ------------ 3. Read Valve Status from Firebase ------------
  bool valve1On = false, valve2On = false, valve3On = false;
   if (Firebase.ready() && auth.token.uid != "") {
    String path;
    for (int i = 1; i <= 3; i++) {
      path = "/water/valves/valve" + String(i);
      if (Firebase.RTDB.getString(&fbdo, path)) {
        String status = fbdo.stringData();
        status.replace("\"", "");
        int pin = (i == 1) ? valve1Pin : (i == 2) ? valve2Pin : valve3Pin;
        bool isOn = (status == "\\ON\\");
        if (i == 1) valve1On = isOn;
        if (i == 2) valve2On = isOn;
        if (i == 3) valve3On = isOn;
        digitalWrite(pin, isOn ? HIGH : LOW);
        Serial.println("Valve " + String(i) + ": " + status);
      }
      else {
        Serial.print("Failed to read ");
        Serial.print(path);
        Serial.print(": ");
        Serial.println(fbdo.errorReason());
    }
  }
}

  // ------------ 4. Control Pump 2 Based on Valve Status ------------
  if (valve1On || valve2On || valve3On) {
    digitalWrite(pump2Pin, HIGH);
    Serial.println("🔁 At least one valve ON → Pumps ON");
  } else {
    digitalWrite(pump2Pin, LOW);
    Serial.println("✅ All valves OFF → Pumps OFF");
  }

  delay(1000); // Loop Delay
}
