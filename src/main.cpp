#include "HardwareSerial.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <cstdint>


WiFiMulti wifiMulti;

// WiFi connect timeout per AP. Increase when connecting takes longer.
const uint32_t connectTimeoutMs = 10000;

void setup(){
  Serial.begin(115200);
  delay(10);
  WiFi.mode(WIFI_STA);
  
  // Add list of wifi networks
  wifiMulti.addAP("WC Devices", "iujonmhmjm");


  // Connect to Wi-Fi using wifiMulti (connects to the SSID with strongest connection)
  Serial.println("Connecting Wifi...");
  if(wifiMulti.run() == WL_CONNECTED) {
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wifi unable to connect");
  }
}

void loop(){
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}