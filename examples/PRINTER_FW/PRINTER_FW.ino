/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * Please install the following dependent libraries before compiling:
 * M5Atom: https://github.com/m5stack/M5Atom
 * FastLED: https://github.com/FastLED/FastLED
 * PubSubClient: https://github.com/knolleary/pubsubclient
 * ArduinoJson: https://github.com/bblanchon/ArduinoJson
 * @Hardwares: Atom Printer
 * @Platform Version: Arduino M5Stack Board Manager v2.1.4
 */

/*
  How to use:
  1. connect to AP `ATOM_PRINTER-xxxx`
  2. Visit 192.168.4.1 to print
  3. Configure WiFi connection and print data through mqtt server (refer README)
*/

#include <M5Atom.h>
#include "ATOM_PRINTER.h"
#include "ATOM_PRINTER_CONFIG.h"
#include "ATOM_PRINTER_WEB.h"
#include "ATOM_PRINTER_MQTT.h"
#include "ATOM_PRINTER_WIFI.h"
#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

xSemaphoreHandle xMQTTMutex = xSemaphoreCreateMutex();

Preferences preferences;
ATOM_PRINTER printer;
DynamicJsonDocument payload(1024);
WebServer webServer(80);
DNSServer dnsServer;
const IPAddress apIP(192, 168, 4, 1);

uint8_t bmp_buffer[BMP_BUFFER_LIMIT] = {0};
int bmp_data_offset                  = 0;
int bmp_data_size                    = 0;
int bmp_width                        = 0;
int bmp_height                       = 0;

// wifi设置
const char *apSSID   = "ATOM_PRINTER";
String wifi_ssid     = "";
String wifi_password = "";
String ssid_html;

bool is_config_mode = true;

String device_mac;

// mqtt
String mqtt_broker   = MQTT_BROKER;
int mqtt_port        = MQTT_PORT;
String mqtt_id       = MQTT_ID;
String mqtt_user     = MQTT_USER;
String mqtt_password = MQTT_PASSWORD;
String mqtt_topic    = MQTT_TOPIC;

bool mqtt_connect_change_event = false;

WiFiClient plainClient;
WiFiClientSecure secureClient;  // used when the broker port is 8883
PubSubClient mqttClient(plainClient);

Atom_Printer_State_t device_state = kInit;

void flashing(uint32_t color, uint8_t frequency)
{
    static uint32_t prev_ms = millis();
    static bool rgbState    = 0;
    if (millis() > prev_ms + frequency) {
        prev_ms  = millis();
        rgbState = !rgbState;
    }
    M5.dis.drawpix(0, color * rgbState);
}

void TaskLED(void *pvParameters)
{
    while (1) {
        switch (device_state) {
            case kInit:
                // M5.dis.drawpix(0, 0xffe500);  //yellow
                flashing(0x00ff00, 20);  // blinking green
                break;
            case kWiFiConnected:
                M5.dis.drawpix(0, 0x00ff00);  // green
                break;
            case kWiFiDisconnected:
                flashing(0xff0000, 20);  // blinking red
                break;
            case kMQTTConnected:
                M5.dis.drawpix(0, 0x0000ff);  // blue
                break;
            case kMQTTDisconnected:
                flashing(0x0000ff, 20);  // blinking blue
                break;
        }
        vTaskDelay(500);
    }
}

void mqttCallback(char *topic, byte *payload, unsigned int len)
{
    Serial.println(mqtt_topic + ":");
    Serial.printf("len:%d\r\n", len);

    // ESC/POS raster image: GS v 0 = 0x1D 0x76 0x30
    // Sent by the Telegram bot's image handler as a raw binary payload.
    if (len >= 8 && payload[0] == 0x1D && payload[1] == 0x76 && payload[2] == 0x30) {
        Serial.println("Printing ESC/POS image...");
        // Tall images arrive as several strips; the bot appends the paper
        // feed to the last one, so don't feed here or the strips get gaps.
        printer.init();
        printer.printRaw(payload, len);
        return;
    }

    // "RAW:" + ESC/POS bytes: written to the printer untouched. The bot uses
    // this to mix text and drawn graphics (e.g. a heart divider) in one print.
    if (len > 4 && memcmp(payload, "RAW:", 4) == 0) {
        Serial.println("Printing raw ESC/POS...");
        printer.printRaw(payload + 4, len - 4);
        return;
    }

    // Text commands. Copied to a heap String: payloads can be up to the
    // 30 KB MQTT buffer, far more than the loop task's 8 KB stack.
    if (len > MAX_TEXT_PAYLOAD) {
        Serial.println("Text payload too large, ignored");
        return;
    }
    String Type;
    Type.reserve(len);
    for (unsigned int i = 0; i < len && payload[i] != '\0'; i++) {
        Type += (char)payload[i];
    }
    Serial.println(Type);

    if (Type.startsWith("TEXT,")) {
        // TEXT,<posx>,<font>:<text>
        int comma = Type.indexOf(',', 5);
        int colon = Type.indexOf(':', 5);
        if (comma < 0 || colon < 0 || comma > colon) {
            Serial.println("Malformed TEXT command, ignored");
            return;
        }
        int posx  = Type.substring(5, comma).toInt();
        int fonts = Type.substring(comma + 1, colon).toInt();
        printer.init();
        printer.printPos(posx);
        printer.fontSize(fonts);
        printer.printASCII(Type.substring(colon + 1));
        printer.newLine(3);
    } else if (Type.startsWith("QR:")) {
        printer.init();
        printer.printQRCode(Type.substring(3));
        printer.newLine(3);
    } else if (Type.startsWith("BAR:")) {
        printer.init();
        printer.setBarCodeHRI(HIDE);
        printer.printBarCode(CODE128, Type.substring(4));
        printer.newLine(3);
    }
}

void setup()
{
    M5.begin(true, false, true);
    printer.begin();
    M5.dis.drawpix(0, 0x00ffff);  // 初始化状态灯
    preferences.begin("PRINTER_CONFIG");
    // disableCore0WDT();
    printer.init();
    // printer.newLine(1);
    // Create LED Task
    xTaskCreatePinnedToCore(TaskLED, "TaskLED"  // A name just for humans
                            ,
                            2048  // This stack size can be checked & adjusted
                                  // by reading the Stack Highwater
                            ,
                            NULL,
                            3  // Priority, with 3 (configMAX_PRIORITIES - 1)
                               // being the highest, and 0 being the lowest.
                            ,
                            NULL, 0);

    wifiInit();
    ssid_html = wifiScan();
    webServerInit();
    device_mac = WiFi.softAPmacAddress();
    mqttClient.setBufferSize(30720);  // 30 KB — fits a full-height 384 px-wide ESC/POS image
    mqttClient.setCallback(mqttCallback);
    // Printing a full 30 KB image at 9600 baud blocks mqttClient.loop() for ~30 s;
    // a short keepalive makes the broker drop the connection mid-print.
    mqttClient.setKeepAlive(60);
    // Bound how long a connect attempt to an unreachable broker can block loop()
    mqttClient.setSocketTimeout(5);
#ifdef MQTT_ROOT_CA
    secureClient.setCACert(MQTT_ROOT_CA);
#else
    secureClient.setInsecure();  // encrypted, but the broker's certificate isn't verified
#endif

    if (preferences.getString("WIFI_SSID").length() > 1) {
        wifi_ssid     = preferences.getString("WIFI_SSID");
        wifi_password = preferences.getString("WIFI_PWD");
        Serial.println("Get WIFI INFO From Preference: " + wifi_ssid);
    }
    Serial.println(mqtt_broker);
}

// Never block here for long: the web server, DNS and button are serviced
// from this loop too.
void loop()
{
    static unsigned long last_wifi_attempt = 0;
    static bool wifi_attempted             = false;
    static unsigned long last_mqtt_attempt = 0;
    static unsigned long wifi_up_since     = 0;
    static bool ap_on                      = true;

    webServer.handleClient();
    if (ap_on) dnsServer.processNextRequest();

    if (WiFi.status() == WL_CONNECTED) {
        if (wifi_up_since == 0) {
            wifi_up_since = millis();
            if (device_state == kInit || device_state == kWiFiDisconnected) device_state = kWiFiConnected;
        }
        if (ap_on && millis() - wifi_up_since > AP_OFF_AFTER_MS) {
            Serial.println("WiFi connected, turning setup access point off");
            dnsServer.stop();
            WiFi.softAPdisconnect(true);
            ap_on = false;
        }

        if (!mqttClient.connected()) {
            if (millis() - last_mqtt_attempt > 5000) {
                last_mqtt_attempt = millis();
                Serial.println("reconnect mqtt...");
                mqttConnect(mqtt_broker, mqtt_port, mqtt_id, mqtt_user, mqtt_password, 0);
            }
        } else {
            mqttClient.loop();
        }
    } else {
        wifi_up_since = 0;
        // Start a connection attempt and check on it in later loops instead of waiting
        if (wifi_ssid != "" && (!wifi_attempted || millis() - last_wifi_attempt > 15000)) {
            wifi_attempted    = true;
            last_wifi_attempt = millis();
            Serial.println("connecting to WiFi " + wifi_ssid);
            WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
            device_state = kWiFiDisconnected;
        }
    }
    if (M5.Btn.pressedFor(5000)) {
        preferences.clear();
        Serial.println("reset device...");
        esp_restart();
    }
    M5.update();
}

// print bmp

// printer.init();x
// printer.printASCII("M5STACK");
// delay(2000);
// printer.init();
// printer.setBarCodeHRI(ABOVE);
// printer.printBarCode(CODE128, "M5STACK");
// delay(2000);
// printer.init();
// printer.printQRCode("M5STACK");
// delay(2000);
// printer.init();
// printer.printBMP(0, 184, 180, bitbuffer2);