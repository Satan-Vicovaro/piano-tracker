#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Ping.h>
#include <ArduinoOTA.h>
#include <TelnetStream.h>
#include <ESPmDNS.h>

const char* ssid = "xxx";
const char* password = "xxx";
WiFiServer server(2137);

void logPrint(const String& msg) {
  Serial.print(msg);
  TelnetStream.print(msg);
}
void logPrintln(const String& msg) {
  Serial.println(msg);
  TelnetStream.println(msg);
}

void setupOTA() {
  ArduinoOTA.setHostname("esp32s3");
  ArduinoOTA.setPassword("admin123");

  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    logPrintln("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    logPrintln("\nUpdate Finished! Rebooting...");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) logPrintln("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) logPrintln("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) logPrintln("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) logPrintln("Receive Failed");
    else if (error == OTA_END_ERROR) logPrintln("End Failed");
  });
  ArduinoOTA.begin();
}

// 1. Check local Wi-Fi connection
bool isWiFiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

// 2. Check DNS resolution to the outside world
bool isDnsWorking() {
  IPAddress resolvedIP;
  return WiFi.hostByName("google.com", resolvedIP) == 1;
}

// 3. Full Internet probe (Captive-portal detection style)
bool hasInternetAccess() {
  if (!isWiFiConnected()) return false;

  HTTPClient http;
  http.setTimeout(3000); // 3 second timeout
  http.begin("http://clients3.google.com/generate_204");

  int httpCode = http.GET();
  http.end();

  // 204 means full uninterrupted internet access
  return (httpCode == 204);
}

void ping_me() { 
  bool success = Ping.ping("10.20.10.34");
  if (success) {
    Serial.printf("Ping OK! Average time: %.1f ms\n", Ping.averageTime());
  }
}


void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.printf("Connecting to %s", ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  TelnetStream.begin();
  logPrintln("\nWiFi connected!");
  Serial.print("Local IP: ");
  logPrintln(WiFi.localIP().toString());
  
  setupOTA();
  logPrintln("OTA service ready!");

  if (MDNS.begin("esp32s3")) {
    Serial.println("\nmDNS responder started: esp32s3.local");
  }
  // (Optional) Advertise what services the ESP provides
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("telnet", "tcp", 23);

  server.begin();
}


void listen_for_user() { 
  WiFiClient client = server.available();

  if (!client) {
      return;
  }

  if(!client.connected()) {
      return;
  }

  
 }

void loop() {
  // OTA must be handled continuously on every loop iteration
  ArduinoOTA.handle();

  // Run network diagnostics every 5 seconds (non-blocking)
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= 5000) {
    lastCheck = millis();

    logPrintln("\n--- Checking Connectivity ---");

    if (isWiFiConnected()) {
      Serial.printf("[OK] Wi-Fi connected (RSSI: %d dBm)\n", WiFi.RSSI());
    } else {
      logPrintln("[FAIL] Wi-Fi disconnected!");
    }

    if (isDnsWorking()) {
      logPrintln("[OK] DNS resolution functional");
    } else {
      logPrintln("[FAIL] DNS lookup failed");
    }

    if (hasInternetAccess()) {
      logPrintln("[OK] Active Internet access confirmed (HTTP 204)");
      neopixelWrite(38, 0, 50, 0); // Green LED on GPIO 38
      logPrintln(WiFi.localIP().toString());
    } else {
        logPrintln("[FAIL] No active internet access");
        neopixelWrite(38, 50, 0, 0); // Red LED on GPIO 38
    }

    ping_me();
  }
}
