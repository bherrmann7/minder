#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <time.h>

const int BUTTON_PIN1 = D2;  // AM button
const int LED_PIN1 = D5;     // AM LED
const int BUTTON_PIN2 = D1;  // PM button
const int LED_PIN2 = D6;     // PM LED

// Settings (loaded from flash)
int amHour = 7;
int amMinute = 30;
int pmHour = 20;
int pmMinute = 0;
int timezoneOffset = -5;

bool amLedState = false;
bool pmLedState = false;
bool lastButtonState1 = HIGH;
bool lastButtonState2 = HIGH;
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
const unsigned long debounceDelay = 50;

bool amTriggeredToday = false;
bool pmTriggeredToday = false;
int lastHour = -1;

unsigned long bothButtonsHeldSince = 0;
bool bothButtonsHeld = false;

ESP8266WebServer server(80);

void saveSettings() {
  File f = LittleFS.open("/config.txt", "w");
  if (f) {
    f.println(amHour);
    f.println(amMinute);
    f.println(pmHour);
    f.println(pmMinute);
    f.println(timezoneOffset);
    f.close();
    Serial.println("Settings saved");
  }
}

void loadSettings() {
  if (LittleFS.exists("/config.txt")) {
    File f = LittleFS.open("/config.txt", "r");
    if (f) {
      amHour = f.readStringUntil('\n').toInt();
      amMinute = f.readStringUntil('\n').toInt();
      pmHour = f.readStringUntil('\n').toInt();
      pmMinute = f.readStringUntil('\n').toInt();
      timezoneOffset = f.readStringUntil('\n').toInt();
      f.close();
      Serial.println("Settings loaded");
    }
  }
}

String formatTime(int h, int m) {
  String result = "";
  String period = "AM";
  
  if (h == 0) {
    h = 12;
  } else if (h == 12) {
    period = "PM";
  } else if (h > 12) {
    h = h - 12;
    period = "PM";
  }
  
  result += String(h);
  result += ":";
  if (m < 10) result += "0";
  result += String(m);
  result += " " + period;
  
  return result;
}

void handleRoot() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  
  String amStatus = amLedState ? "WAITING" : (amTriggeredToday ? "DONE" : "NOT YET");
  String pmStatus = pmLedState ? "WAITING" : (pmTriggeredToday ? "DONE" : "NOT YET");
  
  int amDisplayHour = amHour % 12;
  if (amDisplayHour == 0) amDisplayHour = 12;
  bool amIsAM = amHour < 12;
  
  int pmDisplayHour = pmHour % 12;
  if (pmDisplayHour == 0) pmDisplayHour = 12;
  bool pmIsAM = pmHour < 12;
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Minder</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; max-width: 400px; margin: 20px auto; padding: 0 10px; }";
  html += "h1 { color: #333; }";
  html += ".status { background: #f0f0f0; padding: 15px; border-radius: 8px; margin-bottom: 20px; }";
  html += ".status p { margin: 5px 0; }";
  html += "form { background: #e8f4e8; padding: 15px; border-radius: 8px; }";
  html += "label { display: block; margin-top: 10px; font-weight: bold; }";
  html += ".time-select { display: flex; gap: 5px; margin-top: 5px; }";
  html += "select { padding: 8px; }";
  html += "button { background: #4CAF50; color: white; padding: 12px; border: none; width: 100%; margin-top: 15px; border-radius: 4px; cursor: pointer; }";
  html += "button:hover { background: #45a049; }";
  html += "</style></head><body>";
  
  html += "<h1>Minder</h1>";
  
  html += "<div class='status'>";
  html += "<p><strong>Current time:</strong> " + formatTime(t->tm_hour, t->tm_min) + "</p>";
  html += "<p><strong>AM reminder:</strong> " + amStatus + "</p>";
  html += "<p><strong>PM reminder:</strong> " + pmStatus + "</p>";
  html += "</div>";
  
  html += "<form method='POST' action='/save'>";
  
  // AM time picker
  html += "<label>AM reminder time:</label>";
  html += "<div class='time-select'>";
  html += "<select name='am_hour'>";
  for (int i = 1; i <= 12; i++) {
    html += "<option value='" + String(i) + "'";
    if (i == amDisplayHour) html += " selected";
    html += ">" + String(i) + "</option>";
  }
  html += "</select>";
  html += "<select name='am_min'>";
  for (int i = 0; i < 60; i += 5) {
    html += "<option value='" + String(i) + "'";
    if (i == amMinute) html += " selected";
    html += String(">") + (i < 10 ? "0" : "") + String(i) + "</option>";
  }
  html += "</select>";
  html += "<select name='am_period'>";
  html += "<option value='AM'";
  if (amIsAM) html += " selected";
  html += ">AM</option>";
  html += "<option value='PM'";
  if (!amIsAM) html += " selected";
  html += ">PM</option>";
  html += "</select>";
  html += "</div>";
  
  // PM time picker
  html += "<label>PM reminder time:</label>";
  html += "<div class='time-select'>";
  html += "<select name='pm_hour'>";
  for (int i = 1; i <= 12; i++) {
    html += "<option value='" + String(i) + "'";
    if (i == pmDisplayHour) html += " selected";
    html += ">" + String(i) + "</option>";
  }
  html += "</select>";
  html += "<select name='pm_min'>";
  for (int i = 0; i < 60; i += 5) {
    html += "<option value='" + String(i) + "'";
    if (i == pmMinute) html += " selected";
    html += String(">") + (i < 10 ? "0" : "") + String(i) + "</option>";
  }
  html += "</select>";
  html += "<select name='pm_period'>";
  html += "<option value='AM'";
  if (pmIsAM) html += " selected";
  html += ">AM</option>";
  html += "<option value='PM'";
  if (!pmIsAM) html += " selected";
  html += ">PM</option>";
  html += "</select>";
  html += "</div>";
  
  // Timezone
  html += "<label>Timezone (UTC offset):</label>";
  html += "<select name='tz' style='width: 100%; margin-top: 5px; padding: 8px;'>";
  for (int i = -12; i <= 12; i++) {
    html += "<option value='" + String(i) + "'";
    if (i == timezoneOffset) html += " selected";
    html += ">" + String(i >= 0 ? "+" : "") + String(i) + "</option>";
  }
  html += "</select>";
  
  html += "<button type='submit'>Save Settings</button>";
  html += "</form>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("am_hour") && server.hasArg("am_min") && server.hasArg("am_period")) {
    int h = server.arg("am_hour").toInt();
    int m = server.arg("am_min").toInt();
    String period = server.arg("am_period");
    
    if (period == "AM") {
      amHour = (h == 12) ? 0 : h;
    } else {
      amHour = (h == 12) ? 12 : h + 12;
    }
    amMinute = m;
  }
  
  if (server.hasArg("pm_hour") && server.hasArg("pm_min") && server.hasArg("pm_period")) {
    int h = server.arg("pm_hour").toInt();
    int m = server.arg("pm_min").toInt();
    String period = server.arg("pm_period");
    
    if (period == "AM") {
      pmHour = (h == 12) ? 0 : h;
    } else {
      pmHour = (h == 12) ? 12 : h + 12;
    }
    pmMinute = m;
  }
  
  if (server.hasArg("tz")) {
    timezoneOffset = server.arg("tz").toInt();
    configTime(timezoneOffset * 3600, 0, "pool.ntp.org");
  }
  
  saveSettings();
  
  amTriggeredToday = false;
  pmTriggeredToday = false;
  
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Saved. Redirecting...");
}

void startConfigPortal() {
  Serial.println("Starting config portal...");
  digitalWrite(LED_PIN1, HIGH);
  digitalWrite(LED_PIN2, HIGH);
  
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.startConfigPortal("Minder-Setup");
  
  digitalWrite(LED_PIN1, LOW);
  digitalWrite(LED_PIN2, LOW);
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  
  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  digitalWrite(LED_PIN1, LOW);
  digitalWrite(LED_PIN2, LOW);
  
  // Initialize filesystem
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
  }
  loadSettings();
  
  // Check for config mode (both buttons held at boot)
  if (digitalRead(BUTTON_PIN1) == LOW && digitalRead(BUTTON_PIN2) == LOW) {
    delay(100);
    if (digitalRead(BUTTON_PIN1) == LOW && digitalRead(BUTTON_PIN2) == LOW) {
      startConfigPortal();
    }
  }
  
  // Connect to WiFi (auto-fallback to config portal if not configured)
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);
  
  // Both LEDs on during WiFi connect attempt
  digitalWrite(LED_PIN1, HIGH);
  digitalWrite(LED_PIN2, HIGH);
  
  if (!wifiManager.autoConnect("Minder-Setup")) {
    Serial.println("Failed to connect, restarting...");
    ESP.restart();
  }
  
  digitalWrite(LED_PIN1, LOW);
  digitalWrite(LED_PIN2, LOW);
  
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  
  // Start mDNS
  if (MDNS.begin("minder")) {
    Serial.println("mDNS started: http://minder.local");
  }
  
  // Set up time
  configTime(timezoneOffset * 3600, 0, "pool.ntp.org");
  
  // Wait for time to sync
  Serial.print("Waiting for NTP");
  while (time(nullptr) < 100000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" done");
  
  // Set up web server
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
  MDNS.update();
  
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  int hour = t->tm_hour;
  int minute = t->tm_min;
  
  // Reset triggers at midnight
  if (hour == 0 && lastHour == 23) {
    amTriggeredToday = false;
    pmTriggeredToday = false;
    amLedState = false;
    pmLedState = false;
  }
  lastHour = hour;
  
  // Time-based LED triggers
  if (hour == amHour && minute >= amMinute && !amTriggeredToday) {
    amLedState = true;
    amTriggeredToday = true;
    Serial.println("AM LED triggered by time");
  }
  if (hour == pmHour && minute >= pmMinute && !pmTriggeredToday) {
    pmLedState = true;
    pmTriggeredToday = true;
    Serial.println("PM LED triggered by time");
  }
  
  // Read buttons
  bool reading1 = digitalRead(BUTTON_PIN1);
  bool reading2 = digitalRead(BUTTON_PIN2);
  
  // Check for both buttons held (config mode)
  if (reading1 == LOW && reading2 == LOW) {
    if (!bothButtonsHeld) {
      bothButtonsHeld = true;
      bothButtonsHeldSince = millis();
    } else if (millis() - bothButtonsHeldSince > 5000) {
      startConfigPortal();
    }
  } else {
    bothButtonsHeld = false;
  }
  
  // AM button toggle
  if (reading1 == LOW && lastButtonState1 == HIGH) {
    if ((millis() - lastDebounceTime1) > debounceDelay) {
      amLedState = !amLedState;
      Serial.println(amLedState ? "AM LED ON" : "AM LED OFF");
      lastDebounceTime1 = millis();
    }
  }
  lastButtonState1 = reading1;
  
  // PM button toggle
  if (reading2 == LOW && lastButtonState2 == HIGH) {
    if ((millis() - lastDebounceTime2) > debounceDelay) {
      pmLedState = !pmLedState;
      Serial.println(pmLedState ? "PM LED ON" : "PM LED OFF");
      lastDebounceTime2 = millis();
    }
  }
  lastButtonState2 = reading2;
  
  // Update LEDs
  digitalWrite(LED_PIN1, amLedState);
  digitalWrite(LED_PIN2, pmLedState);
  
  delay(10);
} 
