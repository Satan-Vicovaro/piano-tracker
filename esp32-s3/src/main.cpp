#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP32Ping.h>
#include <ESPmDNS.h>
#include <EspUsbHost.h>
#include <HTTPClient.h>
#include <TelnetStream.h>
#include <WiFi.h>

#include <secrets.hpp>

#ifndef SSID
#define SSID "default_ssid"
#endif

#ifndef SSID_PASSWORD
#define SSID_PASSWORD "default_password"
#endif

WiFiServer server(2137);

#define MAX_CLIENT_COUNT 4
WiFiClient clients[MAX_CLIENT_COUNT];
bool clientActive[MAX_CLIENT_COUNT] = {false};

static const char* NOTE_NAMES[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

EspUsbHost usb;

void logPrintf(const char* format, ...) __attribute__((format(printf, 1, 2)));

void logPrintf(const char* format, ...) {
  char buffer[256];  // Buffer for formatted string
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  Serial.print(buffer);
  TelnetStream.print(buffer);
}

void logPrint(const String& msg) {
  Serial.print(msg);
  TelnetStream.print(msg);
}
void logPrintln(const String& msg) {
  Serial.println(msg);
  TelnetStream.println(msg);
}

void printNoteName(uint8_t noteNumber) {
  int octave = (noteNumber / 12) - 1;  // 60 -> C4
  int noteIndex = noteNumber % 12;
  logPrintf("%s%d (MIDI %d)\n", NOTE_NAMES[noteIndex], octave, noteNumber);
}

void handleMidiMessage(const EspUsbHostMidiMessage& msg) {
  uint8_t type = msg.status & 0xF0;           // Command (0x90 = Note On, 0x80 = Note Off)
  uint8_t channel = (msg.status & 0x0F) + 1;  // Channels 1 - 16
  uint8_t note = msg.data1;                   // Pitch (0 - 127)
  uint8_t vel = msg.data2;                    // Velocity (0 - 127)
  switch (type) {
    case 0x90:  // Note On
      if (vel == 0) {
        // Many pianos send Note On with velocity 0 instead of Note Off
        logPrintf("[Ch %d] Note OFF: ", channel);
        printNoteName(note);
      } else {
        logPrintf("[Ch %d] Note ON:  ", channel);
        printNoteName(note);
        logPrintf(" | Velocity: %d\n", vel);
      }
      break;
    case 0x80:  // Note Off
      logPrintf("[Ch %d] Note OFF: ", channel);
      printNoteName(note);
      logPrintf(" | Release Vel: %d\n", vel);
      break;
    case 0xB0:           // Control Change (Sustain Pedal, Expression, etc.)
      if (note == 64) {  // CC 64 is Damper / Sustain Pedal
        logPrintf("[Ch %d] Sustain Pedal: %s (%d)\n", channel, (vel >= 64) ? "DOWN" : "UP", vel);
      }
      break;

    default:
      break;
  }
}

void setupOTA() {
  ArduinoOTA.setHostname("esp32s3");

  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    logPrintln("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() { logPrintln("\nUpdate Finished! Rebooting..."); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      logPrintln("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      logPrintln("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      logPrintln("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      logPrintln("Receive Failed");
    else if (error == OTA_END_ERROR)
      logPrintln("End Failed");
  });
  ArduinoOTA.begin();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.printf("Connecting to %s", SSID);
  WiFi.begin(SSID, SSID_PASSWORD);

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
  MDNS.addService("piano-tracker", "tcp", 2137);
  MDNS.addService("telnet", "tcp", 23);

  server.begin();

  logPrintln("piano-server on");

  usb.onDeviceConnected([](const EspUsbHostDeviceInfo& dev) {
    logPrintf("[USB] Device connected at address %d: %s %s (VID: 0x%04X, PID: 0x%04X)\n",
              dev.address, dev.manufacturer, dev.product, dev.vid, dev.pid);
  });
  usb.onDeviceDisconnected([](const EspUsbHostDeviceInfo& dev) {
    logPrintf("[USB] Device disconnected at address %d\n", dev.address);
  });

  usb.onMidiMessage(handleMidiMessage);

  if (!usb.begin()) {
    while (1) {
      logPrintln(String("Failed to start USB Host: ") + String(usb.lastErrorName()));
      delay(1000);
    }
  }
  logPrintln("Usb host on");
}

void handle_new_user() {
  WiFiClient newClient = server.available();
  bool slotFound = false;

  if (!newClient.connected()) {
    return;
  }

  for (int i = 0; i < MAX_CLIENT_COUNT; i++) {
    if (!clientActive[i]) {
      clients[i] = newClient;
      clientActive[i] = true;
      logPrintln(String("Client connected!") + String(i));
      slotFound = true;
      break;
    }
  }

  if (!slotFound) {
    newClient.println("Server is busy. Try again later.");
    newClient.stop();
    logPrintln("Rejected client: server full");
  }
  neopixelWrite(38, 50, 0, 0);  // Red LED on GPIO 38
}

void server_user() {
  static char buffer[1024];
  for (int i = 0; i < MAX_CLIENT_COUNT; i++) {
    if (clientActive[i]) {
      while (clients[i].available()) {
        int readBytes = clients[i].readBytes(buffer, sizeof(buffer));
        clients[i].write(buffer, readBytes);
      }
      if (!clients[i].connected()) {
        clients[i].stop();
        clientActive[i] = false;
        logPrintln(String("Client disconnected: ") + String(i));
      }
    }
  }
}

int counter = 0;
int clientCount = 0;

void loop() {
  ArduinoOTA.handle();

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= 5000) {
    lastCheck = millis();
    logPrintln("Im alive btw " + String(counter++));
  }
  handle_new_user();
  server_user();

  neopixelWrite(38, 1, 1, 1);  // Red LED on GPIO 38
}
