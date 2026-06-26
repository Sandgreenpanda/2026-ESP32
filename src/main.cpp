// #include "HardwareSerial.h"
#include "HardwareSerial.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <cstdint>
//#include "home.h"
// #include <DIYables_ESP32_WebServer.h>

WiFiMulti wifiMulti;

// WiFi connect timeout per AP. Increase when connecting takes longer.
const uint32_t connectTimeoutMs = 10000;


//const char homeHtml[] = {
//#embed "templates/home.html"
//};


// Port 80 for http
WiFiServer server(80);
// Variable to store the HTTP request
String header;

unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

void givePage(WiFiClient client, String page, String stylesheet) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println();
    client.println("<!DOCTYPE html><html>");
    client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    client.println("<link rel=\"icon\" href=\"data:,\">");
    client.println("<style>" + stylesheet + "</style></head><body>" + page + "</body></html>");
    client.println();
}

void setup() {
// pins for mosfet test. TEMP
pinMode(23, OUTPUT);
pinMode(22, OUTPUT);
digitalWrite(22, HIGH);
digitalWrite(23, HIGH);
// End TEMP

    Serial.begin(115200);
    Serial.print(""); // Idk, why, but this stops the garbage output at the start
    WiFi.mode(WIFI_STA);

    // Add list of wifi networks
    wifiMulti.addAP("WC Devices", "iujonmhmjm");

    // Connect to Wi-Fi using wifiMulti (connects to the SSID with strongest connection)
    Serial.println("Connecting Wifi...");
    if (wifiMulti.run() == WL_CONNECTED) {
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Wifi unable to connect");
    }

    Serial.println("Website http://" + WiFi.localIP().toString());
    server.begin();
}

void loop() {
    WiFiClient client = server.available(); // Listen for incoming clients

    if (client) { // If a new client connects,
        currentTime = millis();
        previousTime = currentTime;
        Serial.println("New Client.");                                            // print a message out in the serial port
        String currentLine = "";                                                  // make a String to hold incoming data from the client
        while (client.connected() && currentTime - previousTime <= timeoutTime) { // loop while the client's connected
            currentTime = millis();
            if (client.available()) {   // if there's bytes to read from the client,
                char c = client.read(); // read a byte, then
                Serial.write(c);        // print it out the serial monitor
                header += c;
                if (c == '\n') { // if the byte is a newline character
                    // if the current line is blank, you got two newline characters in a row.
                    // that's the end of the client HTTP request, so send a response:
                    if (currentLine.length() == 0) {
                        // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                        // and a content-type so the client knows what's coming, then a blank line:
                        givePage(
                            client,
                            "<p>WEB PAGE!</p>",
                            "html {font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}"
                            ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}"
                            ".button2 {background-color: #555555;}"
                        );
                        break;
                    } else { // if you got a newline, then clear currentLine
                        currentLine = "";
                    }
                } else if (c != '\r') { // if you got anything else but a carriage return character,
                    currentLine += c;   // add it to the end of the currentLine
                }
            }
        }
        // Clear the header variable
        header = "";
        // Close the connection
        client.stop();
        Serial.println("Client disconnected.");
        Serial.println("");
    }
}
