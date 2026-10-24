// Copy this file to arduino_secrets.h and fill in your Wi-Fi details.
// arduino_secrets.h is git-ignored, so your credentials stay out of the repo.
#pragma once

#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Optional static IP. Pick an address outside your router's DHCP range,
// or reserve this address for the board in the router. Adjust the
// gateway/subnet/DNS to match your network. Comment out the whole
// block to use DHCP instead.
#define WIFI_STATIC_IP "192.168.1.50"
#define WIFI_GATEWAY   "192.168.1.1"
#define WIFI_SUBNET    "255.255.255.0"
#define WIFI_DNS       "192.168.1.1"
