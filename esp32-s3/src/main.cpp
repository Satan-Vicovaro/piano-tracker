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

#define BUFFER_SIZE 256

#pragma pack(push, 1)
enum NoteType : uint8_t { UP, DOWN, UNKNOWN };
struct PianoData {
  uint8_t note;
  uint8_t velocity;
  NoteType type;
};
#pragma pack(pop)

enum class ServerMode : uint8_t { ECHO, PIANO_NOTES };

struct CyclicQueue {
  PianoData data[BUFFER_SIZE];
  size_t head = 0;
  size_t tail = 0;
  size_t count = 0;
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

  bool push(const PianoData& item) {
    portENTER_CRITICAL(&mux);
    if (count >= BUFFER_SIZE) {
      tail = (tail + 1) % BUFFER_SIZE;
      count--;
    }
    data[head] = item;
    head = (head + 1) % BUFFER_SIZE;
    count++;
    portEXIT_CRITICAL(&mux);
    return true;
  }

  bool pop(PianoData* out) {
    portENTER_CRITICAL(&mux);
    if (count == 0) {
      portEXIT_CRITICAL(&mux);
      return false;
    }
    *out = data[tail];
    tail = (tail + 1) % BUFFER_SIZE;
    count--;
    portEXIT_CRITICAL(&mux);
    return true;
  }

  size_t size() {
    portENTER_CRITICAL(&mux);
    size_t c = count;
    portEXIT_CRITICAL(&mux);
    return c;
  }
};

CyclicQueue noteQueue;

void appendPianoData(PianoData* data) {
  if (data != nullptr) {
    noteQueue.push(*data);
  }
}

bool popData(PianoData* data) {
  if (data == nullptr) {
    return false;
  }
  return noteQueue.pop(data);
}

WiFiServer server(2137);
WiFiClient activeClient;
ServerMode currentMode = ServerMode::ECHO;

static const char* NOTE_NAMES[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

EspUsbHost usb;

void logPrintf(const char* format, ...) __attribute__((format(printf, 1, 2)));

void logPrintf(const char* format, ...) {
  char buffer[256];
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

void processMidiEvent(uint8_t channel, uint8_t type, uint8_t note, uint8_t vel) {
  NoteType noteType = UNKNOWN;

  switch (type) {
    case 0x90:  // Note On
      if (vel == 0) {
        noteType = UP;
        logPrintf("[Ch %d] Note OFF: ", channel);
        printNoteName(note);
      } else {
        noteType = DOWN;
        logPrintf("[Ch %d] Note ON:  ", channel);
        printNoteName(note);
        logPrintf(" | Velocity: %d\n", vel);
      }
      break;

    case 0x80:  // Note Off
      noteType = UP;
      logPrintf("[Ch %d] Note OFF: ", channel);
      printNoteName(note);
      logPrintf(" | Release Vel: %d\n", vel);
      break;

    case 0xB0:           // Control Change (Sustain Pedal, Expression, etc.)
      if (note == 64) {  // CC 64 is Damper / Sustain Pedal
        logPrintf("[Ch %d] Sustain Pedal: %s (%d)\n", channel, (vel >= 64) ? "DOWN" : "UP", vel);
      }
      noteType = UNKNOWN;
      break;

    default:
      return;  // Ignore unsupported MIDI messages (Clock, SysEx, Pitch Bend, etc.)
  }

  PianoData data = {note, vel, noteType};
  appendPianoData(&data);
}

void handleMidiMessage(const EspUsbHostMidiMessage& msg) {
  uint8_t type = msg.status & 0xF0;  // Command (0x90 = Note On, 0x80 = Note Off, 0xB0 = CC)
  uint8_t channel = (msg.status & 0x0F) + 1;  // Channels 1 - 16
  uint8_t note = msg.data1;                   // Pitch (0 - 127) or CC number
  uint8_t vel = msg.data2;                    // Velocity (0 - 127) or CC value

  processMidiEvent(channel, type, note, vel);
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

void processModeCommand(const String& cmd, const char* buffer, int bytesRead) {
  switch (currentMode) {
    case ServerMode::ECHO:
      activeClient.write((const uint8_t*)buffer, bytesRead);
      break;

    case ServerMode::PIANO_NOTES:
      if (cmd.equalsIgnoreCase("notes")) {
        uint16_t countHeader = (uint16_t)noteQueue.size();
        activeClient.write((const uint8_t*)&countHeader, sizeof(countHeader));

        PianoData data;
        while (popData(&data)) {
          activeClient.write((const uint8_t*)&data, sizeof(PianoData));
        }
      } else {
        activeClient.println(
            "ERR: Unknown command in piano_notes mode. Commands: 'notes', 'mode "
            "<echo|piano_notes>'");
      }
      break;
  }
}

void handle_network_client() {
  if (!activeClient.connected()) {
    WiFiClient newClient = server.available();
    if (newClient) {
      activeClient = newClient;
      currentMode = ServerMode::ECHO;
      logPrintln("[TCP] Client connected! Default mode: ECHO");
    }
    return;
  }

  WiFiClient extraClient = server.available();
  if (extraClient) {
    extraClient.println("ERR: Server busy. Only 1 receiver allowed.");
    extraClient.stop();
    logPrintln("[TCP] Rejected secondary client: only 1 receiver allowed");
  }

  int avail = activeClient.available();
  if (avail > 0) {
    char buffer[256];
    int toRead = std::min(avail, (int)sizeof(buffer) - 1);
    int bytesRead = activeClient.read((uint8_t*)buffer, toRead);
    if (bytesRead > 0) {
      buffer[bytesRead] = '\0';

      String cmd = String(buffer);
      cmd.trim();

      if (cmd.equalsIgnoreCase("mode echo")) {
        rgbLedWrite(38, 50, 0, 0);
        currentMode = ServerMode::ECHO;
        activeClient.println("OK: mode set to echo");
        logPrintln("[Server] Mode set to ECHO");
      } else if (cmd.equalsIgnoreCase("mode piano_notes")) {
        rgbLedWrite(38, 0, 50, 0);
        currentMode = ServerMode::PIANO_NOTES;
        activeClient.println("OK: mode set to piano_notes");
        logPrintln("[Server] Mode set to PIANO_NOTES");
      } else if (cmd.equalsIgnoreCase("mode")) {
        if (currentMode == ServerMode::ECHO) {
          activeClient.println("MODE: echo");
        } else {
          activeClient.println("MODE: piano_notes");
        }
      } else {
        processModeCommand(cmd, buffer, bytesRead);
      }
    }
  }

  // 4. Check for client disconnection
  if (!activeClient.connected()) {
    activeClient.stop();
    currentMode = ServerMode::ECHO;
    logPrintln("[TCP] Client disconnected");
  }
}

int counter = 0;

void loop() {
  ArduinoOTA.handle();

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= 30000) {
    lastCheck = millis();
    logPrintf("I'm alive btw: %d\n", counter++);
  }
  handle_network_client();
}
