// ============================================================
// ESP32 + PN2222A + Relay — web-controlled traffic light
//
// Wiring (unchanged):
//   VBUS (5V) -> ESP32 VIN, PN2222A Collector, Relay VCC
//   GND       -> ESP32 GND, PN2222A GND*, Relay GND
//   GPIO 18   -> 1k -> Base
//
//   *If common-emitter: emitter -> GND
//    If emitter-follower (your current wiring): emitter -> Relay IN
//
// Wi-Fi:
//   Copy arduino_secrets.example.h to arduino_secrets.h and fill in
//   your network details. Without that file (or if the connection
//   fails) the board starts its own access point instead:
//     SSID: ESP32-Relay    Password: relay1234
//
//   Static IP is optional: define WIFI_STATIC_IP, WIFI_GATEWAY and
//   WIFI_SUBNET (and optionally WIFI_DNS) in arduino_secrets.h.
//   Leave them undefined to use DHCP.
//
// Open http://<board-ip> or http://esp32-relay.local
// In access-point mode: http://192.168.4.1
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#if __has_include("arduino_secrets.h")
#include "arduino_secrets.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

// Optional static IP. Default the DNS server to the gateway.
#if defined(WIFI_STATIC_IP)
#ifndef WIFI_GATEWAY
#error "WIFI_GATEWAY must be set when WIFI_STATIC_IP is set."
#endif
#ifndef WIFI_SUBNET
#error "WIFI_SUBNET must be set when WIFI_STATIC_IP is set."
#endif
#ifndef WIFI_DNS
#define WIFI_DNS WIFI_GATEWAY
#endif
#endif

// --- Hardware -------------------------------------------------
const int RELAY_PIN = 18;

// Set this to match your relay module:
//   true  = active-LOW  (most common: input pulled low turns relay ON)
//   false = active-HIGH (input driven high turns relay ON)
const bool RELAY_ACTIVE_LOW = true;

const int RELAY_ON_LEVEL = RELAY_ACTIVE_LOW ? LOW : HIGH;
const int RELAY_OFF_LEVEL = RELAY_ACTIVE_LOW ? HIGH : LOW;

// --- Network --------------------------------------------------
const char *HOSTNAME = "esp32-relay";
const char *AP_SSID = "ESP32-Relay";
const char *AP_PASSWORD = "relay1234"; // WPA2 requires at least 8 characters
const unsigned long WIFI_TIMEOUT_MS = 20000;

WebServer server(80);
bool relayState = false;

// --- Web interface --------------------------------------------
const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#0b0d0c">
<title>Traffic light</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' rx='7' fill='%23181b19'/%3E%3Ccircle cx='16' cy='8.5' r='3.6' fill='%23ff5147'/%3E%3Ccircle cx='16' cy='16' r='3.6' fill='%23ffbf3b'/%3E%3Ccircle cx='16' cy='23.5' r='3.6' fill='%233ce06a'/%3E%3C/svg%3E">
<style>
  :root {
    --bg: #0b0d0c;
    --ink: #f1f4ee;
    --muted: #98a298;
    --edge: #2f3530;
    --focus: #b9d7ff;
    --lens: clamp(54px, 17vw, 68px);
  }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    min-height: 100vh;
    min-height: 100svh;
    display: grid;
    place-items: center;
    padding: 32px 20px calc(32px + env(safe-area-inset-bottom));
    background: radial-gradient(90% 60% at 50% 0%, #191d1a 0%, transparent 70%) fixed, var(--bg);
    color: var(--ink);
    font-family: "Helvetica Neue", Helvetica, Arial, system-ui, sans-serif;
    -webkit-font-smoothing: antialiased;
    text-rendering: optimizeLegibility;
  }
  .console {
    width: min(100%, 340px);
    display: flex;
    flex-direction: column;
    align-items: center;
    text-align: center;
  }
  .title {
    margin: 0 0 22px;
    font-size: 15px;
    font-weight: 500;
    letter-spacing: 0.01em;
    color: var(--muted);
  }
  .signal {
    display: grid;
    gap: 10px;
    padding: 14px;
    border: 1px solid var(--edge);
    border-radius: 28px;
    background: linear-gradient(155deg, #292f2a 0%, #151916 55%, #101311 100%);
    box-shadow:
      0 30px 50px -28px rgba(0, 0, 0, 0.9),
      0 2px 6px rgba(0, 0, 0, 0.55),
      inset 0 1px 0 rgba(255, 255, 255, 0.07);
  }
  .lamp {
    display: grid;
    place-items: center;
    width: calc(var(--lens) + 16px);
    height: calc(var(--lens) + 16px);
    border-radius: 50%;
    background: #090b09;
    box-shadow:
      inset 0 10px 16px -6px rgba(0, 0, 0, 0.95),
      inset 0 -1px 2px rgba(255, 255, 255, 0.05);
  }
  .lens {
    width: var(--lens);
    height: var(--lens);
    border-radius: 50%;
    background: radial-gradient(circle at 38% 32%, #343a35 0%, #181b18 55%, #0e100f 100%);
    box-shadow:
      inset 0 3px 7px rgba(0, 0, 0, 0.75),
      inset 0 -3px 6px rgba(0, 0, 0, 0.6);
    transition: background 220ms ease, box-shadow 220ms ease;
  }
  .lens-red {
    --lit-hi: #ffb0a8; --lit: #ff5147; --lit-lo: #8c130c;
    --glow: rgba(255, 81, 71, 0.55); --glow-far: rgba(255, 81, 71, 0.18);
  }
  .lens-amber {
    --lit-hi: #ffe0a0; --lit: #ffbf3b; --lit-lo: #8a5c00;
    --glow: rgba(255, 191, 59, 0.5); --glow-far: rgba(255, 191, 59, 0.16);
  }
  .lens-green {
    --lit-hi: #a5f7bc; --lit: #3ce06a; --lit-lo: #0c6a2c;
    --glow: rgba(60, 224, 106, 0.5); --glow-far: rgba(60, 224, 106, 0.16);
  }
  body[data-state="off"] .lens-red,
  body[data-state="on"] .lens-green,
  body[data-state="pending"] .lens-amber {
    background: radial-gradient(circle at 38% 32%, var(--lit-hi) 0%, var(--lit) 42%, var(--lit-lo) 100%);
    box-shadow:
      0 0 18px var(--glow),
      0 0 52px var(--glow-far),
      inset 0 -4px 8px rgba(0, 0, 0, 0.35);
  }
  .status {
    margin: 28px 0 0;
    min-height: 1.3em;
    font-size: 24px;
    font-weight: 600;
    letter-spacing: -0.015em;
  }
  button {
    width: 100%;
    min-height: 54px;
    margin-top: 20px;
    padding: 0 24px;
    border: 1px solid rgba(255, 255, 255, 0.14);
    border-radius: 999px;
    background: linear-gradient(180deg, #f6f8f2 0%, #dfe4da 100%);
    color: #10130f;
    font: inherit;
    font-size: 17px;
    font-weight: 600;
    cursor: pointer;
    touch-action: manipulation;
    box-shadow:
      inset 0 1px 0 rgba(255, 255, 255, 0.6),
      0 6px 16px -8px rgba(0, 0, 0, 0.8);
    transition: transform 80ms ease, filter 160ms ease, opacity 160ms ease;
  }
  button:hover:not(:disabled) { filter: brightness(1.05); }
  button:active:not(:disabled) { transform: translateY(1px); }
  button:disabled { opacity: 0.55; cursor: default; }
  button:focus-visible { outline: 3px solid var(--focus); outline-offset: 3px; }
  .error {
    max-width: 32ch;
    margin: 16px 0 0;
    color: #ff9b93;
    font-size: 14px;
    line-height: 1.5;
  }
  [hidden] { display: none !important; }
  @media (prefers-reduced-motion: no-preference) {
    body[data-state="pending"] .lens-amber {
      animation: amber-pulse 1.6s ease-in-out infinite;
    }
    @keyframes amber-pulse {
      0%, 100% {
        box-shadow:
          0 0 18px var(--glow),
          0 0 52px var(--glow-far),
          inset 0 -4px 8px rgba(0, 0, 0, 0.35);
      }
      50% {
        box-shadow:
          0 0 8px var(--glow-far),
          0 0 16px rgba(0, 0, 0, 0),
          inset 0 -4px 8px rgba(0, 0, 0, 0.35);
      }
    }
  }
</style>
</head>
<body data-state="pending">
<main class="console">
  <h1 class="title">Traffic light</h1>
  <div class="signal" aria-hidden="true">
    <div class="lamp"><div class="lens lens-red"></div></div>
    <div class="lamp"><div class="lens lens-amber"></div></div>
    <div class="lamp"><div class="lens lens-green"></div></div>
  </div>
  <p class="status" role="status">Checking&hellip;</p>
  <button id="toggle" type="button" disabled>Turn light on</button>
  <p class="error" id="error" hidden>Can't reach the relay. Make sure you're on the same Wi-Fi network, then try again.</p>
</main>
<script>
  (function () {
    "use strict";

    var statusEl = document.querySelector(".status");
    var toggleBtn = document.getElementById("toggle");
    var errorEl = document.getElementById("error");

    var labels = {
      checking: "Checking…",
      switching: "Switching…",
      on: "Light on",
      off: "Light off",
      unknown: "Not connected"
    };

    var on = false;
    var view = "checking";
    var epoch = 0;
    var polling = false;
    var switching = false;

    function render() {
      var signalState = (view === "checking" || view === "switching") ? "pending" : view;
      document.body.dataset.state = signalState;
      statusEl.textContent = labels[view];
      toggleBtn.textContent = on ? "Turn light off" : "Turn light on";
      toggleBtn.disabled = !(view === "on" || view === "off");
      errorEl.hidden = view !== "unknown";
    }

    function request(url, options) {
      var controller = new AbortController();
      var timer = setTimeout(function () { controller.abort(); }, 4000);
      var init = Object.assign({ cache: "no-store", signal: controller.signal }, options);
      return fetch(url, init)
        .then(function (res) {
          if (!res.ok) throw new Error("HTTP " + res.status);
          return res.json();
        })
        .finally(function () { clearTimeout(timer); });
    }

    async function refresh() {
      if (polling || switching || document.hidden) return;
      polling = true;
      var started = epoch;
      try {
        var state = await request("/state");
        if (started === epoch) {
          on = state.on === true;
          view = on ? "on" : "off";
        }
      } catch (err) {
        if (started === epoch) view = "unknown";
      }
      polling = false;
      render();
    }

    toggleBtn.addEventListener("click", async function () {
      if (switching) return;
      epoch += 1;
      switching = true;
      view = "switching";
      render();
      try {
        var state = await request("/toggle", { method: "POST" });
        on = state.on === true;
        view = on ? "on" : "off";
      } catch (err) {
        view = "unknown";
      }
      switching = false;
      render();
    });

    document.addEventListener("visibilitychange", function () {
      if (!document.hidden) refresh();
    });

    render();
    refresh();
    setInterval(refresh, 3000);
  })();
</script>
</body>
</html>
)HTML";

// --- Relay ----------------------------------------------------
void setRelay(bool on) {
  relayState = on;
  digitalWrite(RELAY_PIN, on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
}

// --- Wi-Fi ----------------------------------------------------
bool connectToWiFi() {
  if (WIFI_SSID[0] == '\0') {
    Serial.println("No Wi-Fi credentials found in arduino_secrets.h");
    return false;
  }

  Serial.printf("Connecting to \"%s\"", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

#ifdef WIFI_STATIC_IP
  IPAddress localIP;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;
  if (!localIP.fromString(WIFI_STATIC_IP) ||
      !gateway.fromString(WIFI_GATEWAY) ||
      !subnet.fromString(WIFI_SUBNET) ||
      !dns.fromString(WIFI_DNS)) {
    Serial.println("Invalid static IP settings - falling back to DHCP");
  } else if (WiFi.config(localIP, gateway, subnet, dns)) {
    Serial.printf("Using static IP %s\n", WIFI_STATIC_IP);
  } else {
    Serial.println("Static IP config failed - falling back to DHCP");
  }
#else
  Serial.println("Using DHCP");
#endif

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  return WiFi.status() == WL_CONNECTED;
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.printf("Access point \"%s\" started (password: %s)\n", AP_SSID, AP_PASSWORD);
  Serial.printf("Open http://%s\n", WiFi.softAPIP().toString().c_str());
}

// --- HTTP handlers --------------------------------------------
void sendStateJson() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json",
              relayState ? "{\"on\":true}" : "{\"on\":false}");
}

void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleState() {
  sendStateJson();
}

void handleToggle() {
  setRelay(!relayState);
  Serial.printf("Relay %s\n", relayState ? "ON" : "OFF");
  sendStateJson();
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// --- Setup / loop ---------------------------------------------
void setup() {
  // Pre-load the output register BEFORE pinMode to avoid a boot glitch.
  digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL);
  pinMode(RELAY_PIN, OUTPUT);
  setRelay(false);

  Serial.begin(115200);
  Serial.println();
  Serial.println("ESP32 web relay - traffic light control");

  bool online = connectToWiFi();
  if (online) {
    Serial.printf("Connected. IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    startAccessPoint();
  }

  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS: http://%s.local\n", HOSTNAME);
  } else {
    Serial.println("mDNS failed to start");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/state", HTTP_GET, handleState);
  server.on("/toggle", HTTP_POST, handleToggle);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("Relay OFF at boot. Web server ready.");
}

void loop() {
  server.handleClient();
}
