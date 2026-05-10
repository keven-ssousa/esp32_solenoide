#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "wifi_secrets.h"

static const char* const WIFI_SSID     = WIFI_SECRET_SSID;
static const char* const WIFI_PASSWORD = WIFI_SECRET_PASSWORD;

static const IPAddress WIFI_IP  (192, 168,   1,  22);
static const IPAddress WIFI_GW  (192, 168,   1,   1);
static const IPAddress WIFI_SN  (255, 255, 255,   0);
static const IPAddress WIFI_DNS1(  8,   8,   8,   8);
static const IPAddress WIFI_DNS2(  8,   8,   4,   4);

const int RELAY_PIN = 14;

bool valveOpen = false;

WebServer server(80);

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Válvula Solenoide</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: Arial, sans-serif;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      min-height: 100vh;
      background: #1a1a2e;
      color: #eee;
      gap: 1.5rem;
    }
    h1 { font-size: 1.5rem; color: #e94560; text-align: center; }
    .status {
      font-size: 1rem;
      padding: 0.5rem 1.4rem;
      border-radius: 20px;
      background: #16213e;
      border: 1px solid #0f3460;
      transition: color 0.3s, border-color 0.3s;
    }
    .status--open   { color: #4ade80; border-color: #4ade80; }
    .status--closed { color: #f87171; border-color: #f87171; }
    button {
      padding: 1rem 3rem;
      font-size: 1.2rem;
      font-weight: bold;
      color: white;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      transition: transform 0.1s, opacity 0.2s;
    }
    #btnOpen  { background: #16a34a; }
    #btnClose { background: #dc2626; }
    button:hover    { opacity: 0.85; }
    button:active   { transform: scale(0.97); }
    button:disabled { opacity: 0.45; cursor: not-allowed; }
  </style>
</head>
<body>
  <h1>Controle Válvula Solenoide</h1>
  <div class="status status--closed" id="status">Válvula Fechada</div>
  <button id="btnOpen"  onclick="sendCommand('open')">Abrir Válvula</button>
  <button id="btnClose" onclick="sendCommand('close')" disabled>Fechar Válvula</button>
  <script>
    async function sendCommand(route) {
      document.getElementById('btnOpen').disabled  = true;
      document.getElementById('btnClose').disabled = true;
      try {
        const res  = await fetch('/' + route);
        const json = await res.json();
        updateUI(json.open);
      } catch (e) {
        updateUI(false);
      }
    }
    function updateUI(isOpen) {
      const status = document.getElementById('status');
      if (isOpen) {
        status.textContent = 'Válvula Aberta';
        status.className   = 'status status--open';
        document.getElementById('btnOpen').disabled  = true;
        document.getElementById('btnClose').disabled = false;
      } else {
        status.textContent = 'Válvula Fechada';
        status.className   = 'status status--closed';
        document.getElementById('btnOpen').disabled  = false;
        document.getElementById('btnClose').disabled = true;
      }
    }
    fetch('/status').then(r => r.json()).then(j => updateUI(j.open)).catch(() => {});
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void handleOpen() {
  valveOpen = true;
  digitalWrite(RELAY_PIN, LOW);
  Serial.println(">> VALVE OPEN");
  server.send(200, "application/json", "{\"open\":true}");
}

void handleClose() {
  valveOpen = false;
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println(">> VALVE CLOSED");
  server.send(200, "application/json", "{\"open\":false}");
}

void handleStatus() {
  String body = String("{\"open\":") + (valveOpen ? "true" : "false") + "}";
  server.send(200, "application/json", body);
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  Serial.printf("Relay pin: GPIO %d — HIGH (closed)\n", RELAY_PIN);

  WiFi.config(WIFI_IP, WIFI_GW, WIFI_SN, WIFI_DNS1, WIFI_DNS2);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nConnected! Visit: http://%s\n", WiFi.localIP().toString().c_str());

  server.on("/",           handleRoot);
  server.on("/open",       handleOpen);
  server.on("/close",      handleClose);
  server.on("/status",     handleStatus);
  server.on("/favicon.ico", []() { server.send(204); });
  server.onNotFound([]()   { server.send(404); });
  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
}
