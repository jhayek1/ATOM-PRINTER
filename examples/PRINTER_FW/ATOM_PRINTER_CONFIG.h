#ifndef _ATOM_PRINTER_CONFIG_H
#define _ATOM_PRINTER_CONFIG_H

#define DNS_PORT         53
#define BMP_BUFFER_LIMIT 1024 * 50
#define MAX_TEXT_PAYLOAD 4096  // TEXT/QR/BAR commands longer than this are ignored

// Turn the setup access point off this long after WiFi connects, so nobody
// nearby can join it to print or change the WiFi / MQTT settings
#define AP_OFF_AFTER_MS  (2 * 60 * 1000)

// Private broker settings go in ATOM_PRINTER_SECRETS.h (not committed, see
// ATOM_PRINTER_SECRETS.h.example). Without it, the public m5stack broker is
// used, where anyone can read and send messages on any topic.
#if __has_include("ATOM_PRINTER_SECRETS.h")
#include "ATOM_PRINTER_SECRETS.h"
#endif

#ifndef MQTT_BROKER
#define MQTT_BROKER "mqtt.m5stack.com"
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883  // 8883 switches to TLS
#endif
#ifndef MQTT_ID
#define MQTT_ID ""
#endif
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif
#ifndef MQTT_TOPIC
#define MQTT_TOPIC ""  // if "" default use the mac address for subscribe
#endif
#define MQTT_TLS_PORT 8883

typedef enum {
    kInit = 0,
    kWiFiConnected,
    kWiFiDisconnected,
    kMQTTConnected,
    kMQTTDisconnected,
} Atom_Printer_State_t;

#endif