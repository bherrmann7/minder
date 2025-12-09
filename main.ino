#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>

const char* ssid = "herrmanns";
const char* password = "herrmanns";

const int BUTTON_PIN1 = D2;  // AM button
const int LED_PIN1 = D5;     // AM LED
const int BUTTON_PIN2 = D1;  // PM button
const int LED_PIN2 = D6;     // PM LED

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

ESP8266WebServer server(80);

void handleRoot() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  
  String html = "<html><body>";
  html += "<h1>AM/PM Tracker</h1>";
  html += "<p>Current time: " + String(t->tm_hour) + ":" + String(t->tm_min) + "</p>";
  html += "<p>AM LED: " + String(amLedState ? "ON" : "OFF") + "</p>";
  html += "<p>PM LED: " + String(pmLedState ? "ON" : "OFF") + "</p>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  
  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  digitalWrite(LED_PIN1, LOW);
  digitalWrite(LED_PIN2, LOW);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  
  // Set up time (EST = -5 hours, with DST)
  configTime(-5 * 3600, 3600, "pool.ntp.org");
  
  // Wait for time to sync
  Serial.print("Waiting for NTP");
  while (time(nullptr) < 100000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" done");
  
  // Set up web server
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
  
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  int hour = t->tm_hour;
  int minute = t->tm_min;
  
  // Reset triggers at midnight
  if (hour == 0 && lastHour == 23) {
    amTriggeredToday = false;
    pmTriggeredToday = false;
  }
  lastHour = hour;
  
  // Time-based LED triggers (turn on at scheduled time, once per day)
  if (hour == 7 && minute >= 30 && !amTriggeredToday) {
    amLedState = true;
    amTriggeredToday = true;
    Serial.println("AM LED triggered by time");
  }
  if (hour == 20 && !pmTriggeredToday) {
    pmLedState = true;
    pmTriggeredToday = true;
    Serial.println("PM LED triggered by time");
  }
  
  // AM button toggle
  bool reading1 = digitalRead(BUTTON_PIN1);
  if (reading1 == LOW && lastButtonState1 == HIGH) {
    if ((millis() - lastDebounceTime1) > debounceDelay) {
      amLedState = !amLedState;
      Serial.println(amLedState ? "AM LED ON" : "AM LED OFF");
      lastDebounceTime1 = millis();
    }
  }
  lastButtonState1 = reading1;
  
  // PM button toggle
  bool reading2 = digitalRead(BUTTON_PIN2);
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
