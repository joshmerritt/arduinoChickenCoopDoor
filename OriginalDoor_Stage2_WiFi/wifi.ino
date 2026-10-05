// ===== WIFI: status page, Open/Close buttons, phone notifications =====
// Only the ESP8266 has WiFi. On the Uno these functions do nothing.

#if defined(ESP8266)

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

#if __has_include("arduino_secrets.h")
#include "arduino_secrets.h"
#else
#error "Copy arduino_secrets.example.h to a new file named arduino_secrets.h and fill in your WiFi details."
#endif

const char* HOSTNAME = "coopdoor";  // status page at http://coopdoor.local
const unsigned long WIFI_WAIT_MS = 15000;

ESP8266WebServer server(80);
bool announcedAddress = false;
bool mdnsRunning = false;

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.begin(SECRET_WIFI_NAME, SECRET_WIFI_PASSWORD);

  // Wait briefly so the startup notification can go out. The door doesn't
  // need WiFi, and the ESP8266 keeps reconnecting in the background.
  Serial.print(F("Connecting to WiFi"));
  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < WIFI_WAIT_MS) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  server.on("/", HTTP_GET, showStatusPage);
  server.on("/open", HTTP_POST, handleOpenButton);
  server.on("/close", HTTP_POST, handleCloseButton);
  server.begin();
}

void handleWifi() {
  if (WiFi.status() == WL_CONNECTED && !announcedAddress) {
    announcedAddress = true;
    Serial.print(F("Status page: http://"));
    Serial.print(WiFi.localIP());
    Serial.print(F(" or http://"));
    Serial.print(HOSTNAME);
    Serial.println(F(".local"));
    mdnsRunning = MDNS.begin(HOSTNAME);
    if (mdnsRunning) MDNS.addService("http", "tcp", 80);
  }
  if (mdnsRunning) MDNS.update();
  server.handleClient();
}

// ===== STATUS PAGE =====

void showStatusPage() {
  String page;
  page.reserve(2500);
  page += F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width, initial-scale=1'>"
            "<meta http-equiv='refresh' content='30'>"
            "<title>Coop Door</title><style>"
            "body{font-family:sans-serif;max-width:30em;margin:1em auto;padding:0 1em}"
            "td{padding:.4em 1em .4em 0;vertical-align:top}"
            "form{display:inline}"
            "button{font-size:1.2em;padding:.6em 1.2em;margin:1em .5em 0 0}"
            "</style></head><body><h1>Coop Door</h1><table>");
  addRow(page, "Door", doorPositionText());
  addRow(page, "Light reading", String(lightReading) + " (" + lightLevelName() + ")");
  addRow(page, "Opens / closes at", String(BRIGHT_ENOUGH_TO_OPEN) + " or lower / " + DARK_ENOUGH_TO_CLOSE + " or higher");
  addRow(page, "Automatic control", automationDescription());
  if (lastEvent.length() > 0) {
    addRow(page, "Last event", lastEvent + " (" + formatDuration(millis() - lastEventMs) + " ago)");
  }
  addRow(page, "Running for", formatDuration(millis()));
  addRow(page, "WiFi signal", String(WiFi.RSSI()) + " dBm");
  page += F("</table>"
            "<form method='post' action='/open' onsubmit=\"return confirm('Open the door?')\">"
            "<button>Open door</button></form>"
            "<form method='post' action='/close' onsubmit=\"return confirm('Close the door?')\">"
            "<button>Close door</button></form>"
            "</body></html>");
  server.send(200, "text/html", page);
}

void addRow(String& page, const char* label, const String& value) {
  page += "<tr><td>";
  page += label;
  page += "</td><td>";
  page += value;
  page += "</td></tr>";
}

const char* lightLevelName() {
  if (lightReading <= BRIGHT_ENOUGH_TO_OPEN) return "bright";
  if (lightReading >= DARK_ENOUGH_TO_CLOSE) return "dark";
  return "in between";
}

String automationDescription() {
  unsigned long msLeft = pauseTimeLeftMs();
  if (msLeft == 0) return "on";
  return String("paused for ") + formatDuration(msLeft) + ": " + pauseReason;
}

// For example "45 s", "12 min", "3 h 5 min" or "2 d 4 h".
String formatDuration(unsigned long ms) {
  unsigned long minutes = ms / 60000;
  unsigned long hours = minutes / 60;
  if (hours >= 24) return String(hours / 24) + " d " + hours % 24 + " h";
  if (hours > 0) return String(hours) + " h " + minutes % 60 + " min";
  if (minutes > 0) return String(minutes) + " min";
  return String(ms / 1000) + " s";
}

// ===== OPEN / CLOSE BUTTONS =====

void handleOpenButton() {
  queueCommand(OPEN_COMMAND);
}

void handleCloseButton() {
  queueCommand(CLOSE_COMMAND);
}

// loop() runs the command. The browser goes back to the status page, which
// finishes loading once the door has stopped moving.
void queueCommand(int command) {
  if (strlen(SECRET_WEB_PASSWORD) > 0 && !server.authenticate(SECRET_WEB_USERNAME, SECRET_WEB_PASSWORD)) {
    server.requestAuthentication();
    return;
  }
  pendingCommand = command;
  server.sendHeader("Location", "/");
  server.send(303);
}

// ===== PHONE NOTIFICATIONS =====

// Sends a push notification through ntfy.sh to the topic in arduino_secrets.h.
void sendPhoneNotification(const String& message, bool isProblem) {
  if (strlen(SECRET_NTFY_TOPIC) == 0 || WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();  // encrypted, but doesn't check ntfy.sh's certificate
  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(client, String("https://ntfy.sh/") + SECRET_NTFY_TOPIC)) return;
  http.addHeader("Title", "Coop door");
  http.addHeader("Tags", isProblem ? "warning" : "chicken");
  http.addHeader("Priority", isProblem ? "high" : "default");
  int status = http.POST(message);
  http.end();
  if (status != HTTP_CODE_OK) {
    Serial.print(F("Notification failed: "));
    Serial.println(status);
  }
}

#else  // Arduino Uno: no WiFi, so these do nothing.

void setupWifi() {}
void handleWifi() {}
void sendPhoneNotification(const String&, bool) {}

#endif
