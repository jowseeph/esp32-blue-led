#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ==================== CONFIG ====================
const char* WIFI_SSID     = "Joshua-2.4G";
const char* WIFI_PASSWORD = "joshua762002";

const char* SUPABASE_URL  = "https://vszeihhsgzmqadnrwckq.supabase.co";
const char* SUPABASE_KEY  = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InZzemVpaGhzZ3ptcWFkbnJ3Y2txIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzQ0MzM0OTYsImV4cCI6MjA5MDAwOTQ5Nn0.w3u5N-uRh6YZeLddDYRM1WJ2Ij-ds_Y-tDTBvlzPn40";

#define LED_PIN        2
#define POLL_INTERVAL  3000     // Poll every 3 seconds

// ==================== GLOBAL VARIABLES ====================
bool lastLedStatus = false;
unsigned long lastPollTime = 0;

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("\n=== ESP32 Supabase LED Controller ===");
  connectToWiFi();
}

// ==================== MAIN LOOP ====================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Disconnected! Reconnecting...");
    connectToWiFi();
    delay(1000);
    return;
  }

  unsigned long now = millis();

  // Poll Supabase for updates
  if (now - lastPollTime >= POLL_INTERVAL) {
    lastPollTime = now;
    fetchLedStatus();
  }
}

// ==================== WiFi Connection ====================
void connectToWiFi() {
  Serial.print("[WiFi] Connecting to: ");
  Serial.println(WIFI_SSID);
  
  WiFi.disconnect(true);
  delay(1000);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected successfully!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] Connection failed.");
  }
}

// ==================== READ LED Status from Supabase ====================
void fetchLedStatus() {
  HTTPClient http;
  String endpoint = String(SUPABASE_URL) + "/rest/v1/led_control?id=eq.1&select=led_status";
  
  http.begin(endpoint);
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.print("[Supabase] Response: ");
    Serial.println(payload);

    DynamicJsonDocument doc(256);
    DeserializationError err = deserializeJson(doc, payload);

    if (!err && doc.size() > 0) {
      bool currentStatus = doc[0]["led_status"].as<bool>();
      
      if (currentStatus != lastLedStatus) {
        lastLedStatus = currentStatus;
        digitalWrite(LED_PIN, currentStatus ? HIGH : LOW);
        Serial.println(currentStatus ? "[LED] Turned ON (from Supabase)" : "[LED] Turned OFF (from Supabase)");
      }
    }
  } else {
    Serial.print("[HTTP GET Error] Code: ");
    Serial.println(httpCode);
  }
  
  http.end();
}

// ==================== SEND LED Status to Supabase (PATCH) ====================
void sendLedStatus(bool newStatus) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Not connected, cannot send update");
    return;
  }

  HTTPClient http;
  String endpoint = String(SUPABASE_URL) + "/rest/v1/led_control?id=eq.1";

  http.begin(endpoint);
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");

  String payload = "{\"led_status\":" + String(newStatus ? "true" : "false") + "}";

  int httpCode = http.PATCH(payload);

  if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_NO_CONTENT) {
    Serial.println(newStatus ? "[Supabase] Updated → ON" : "[Supabase] Updated → OFF");
  } else {
    Serial.print("[PATCH Failed] Code: ");
    Serial.println(httpCode);
    Serial.println("Response: " + http.getString());
  }

  http.end();
}

// ==================== MANUAL TOGGLE FUNCTION (You can call this from a button later) ====================
void toggleLED() {
  bool newStatus = !lastLedStatus;
  
  digitalWrite(LED_PIN, newStatus ? HIGH : LOW);
  lastLedStatus = newStatus;

  Serial.println(newStatus ? "[LED] Manually toggled → ON" : "[LED] Manually toggled → OFF");
  
  sendLedStatus(newStatus);   // Sync to Supabase
}