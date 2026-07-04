


#include "esp32-hal.h"
#include <sha/sha_parallel_engine.h>
#include <WiFi.h>
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <WiFiMulti.h>

#define LAPTOP_HP_1 17
#define LAPTOP_HP_2 16
#define LAPTOP_LENOVO_1 4
#define LAPTOP_ACER_1 19
#define LAPTOP_ACER_2 18


// The HTTPS Server comes in a separate namespace. For easier use, include it here.
using namespace httpsserver;

WiFiMulti wifiMulti;

SSLCert * cert;
HTTPSServer * secureServer;

// Declare some handler functions for the various URLs on the server
void handleRoot(HTTPRequest * req, HTTPResponse * res);
void handle404(HTTPRequest * req, HTTPResponse * res);
void handleTerminal(HTTPRequest * req, HTTPResponse * res);
void handleToggle1(HTTPRequest * req, HTTPResponse * res);
void handleToggle2(HTTPRequest * req, HTTPResponse * res);
void handleToggle3(HTTPRequest * req, HTTPResponse * res);
void handleToggle4(HTTPRequest * req, HTTPResponse * res);
void handleToggle5(HTTPRequest * req, HTTPResponse * res);

void setup() {
  Serial.begin(115200);
  delay(3000);


digitalWrite(LAPTOP_HP_1, LOW);
digitalWrite(LAPTOP_HP_2, LOW);
digitalWrite(LAPTOP_LENOVO_1, LOW);
digitalWrite(LAPTOP_ACER_1, LOW);
digitalWrite(LAPTOP_ACER_2, LOW);
pinMode(LAPTOP_HP_1, OUTPUT);
pinMode(LAPTOP_HP_2, OUTPUT);
pinMode(LAPTOP_LENOVO_1, OUTPUT);
pinMode(LAPTOP_ACER_1, OUTPUT);
pinMode(LAPTOP_ACER_2, OUTPUT);

  Serial.println("Creating a new self-signed certificate.");
  Serial.println("This may take up to a minute, so be patient ;-)");


  cert = new SSLCert();

  int createCertResult = createSelfSignedCert(
    *cert,
    KEYSIZE_1024,
    "CN=10.47.6.92,O=Sandgreenpanda,C=DE",
    "20190101000000",
    "20300101000000"
  );

  // Now check if creating that worked
  if (createCertResult != 0) {
    Serial.printf("Cerating certificate failed. Error Code = 0x%02X, check SSLCert.hpp for details", createCertResult);
    while(true) delay(500);
  }
  Serial.println("Creating the certificate was successful");

  // If you're working on a serious project, this would be a good place to initialize some form of non-volatile storage
  // and to put the certificate and the key there. This has the advantage that the certificate stays the same after a reboot
  // so your client still trusts your server, additionally you increase the speed-up of your application.
  // Some browsers like Firefox might even reject the second run for the same issuer name (the distinguished name defined above).
  //
  // Storing:
  //   For the key:
  //     cert->getPKLength() will return the length of the private key in byte
  //     cert->getPKData() will return the actual private key (in DER-format, if that matters to you)
  //   For the certificate:
  //     cert->getCertLength() and ->getCertData() do the same for the actual certificate data.
  // Restoring:
  //   When your applications boots, check your non-volatile storage for an existing certificate, and if you find one
  //   use the parameterized SSLCert constructor to re-create the certificate and pass it to the HTTPSServer.
  //
  // A short reminder on key security: If you're working on something professional, be aware that the storage of the ESP32 is
  // not encrypted in any way. This means that if you just write it to the flash storage, it is easy to extract it if someone
  // gets a hand on your hardware. You should decide if that's a relevant risk for you and apply countermeasures like flash
  // encryption if neccessary

  // We can now use the new certificate to setup our server as usual.
  secureServer = new HTTPSServer(cert);

  // Connect to WiFi
  Serial.println("Setting up WiFi");
 wifiMulti.addAP("WC Devices", "iujonmhmjm");
 wifiMulti.addAP("SPARK-UMRK2N", "NKUWEW7ZZV");

    // Connect to Wi-Fi using wifiMulti (connects to the SSID with strongest connection)
    Serial.println("Connecting Wifi...");
    if (wifiMulti.run() == WL_CONNECTED) {
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Wifi unable to connect");
    }

    Serial.println("Website http://" + WiFi.localIP().toString());

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("Connected. IP=");
  Serial.println(WiFi.localIP());

  // For every resource available on the server, we need to create a ResourceNode
  // The ResourceNode links URL and HTTP method to a handler function
  ResourceNode * nodeRoot    = new ResourceNode("/", "GET", &handleRoot);
  ResourceNode * node404     = new ResourceNode("", "GET", &handle404);
  ResourceNode * nodeTerminal = new ResourceNode("/terminal", "GET", &handleTerminal);
  ResourceNode * nodeToggle1 = new ResourceNode("/toggle1", "POST", &handleToggle1);
  ResourceNode * nodeToggle2 = new ResourceNode("/toggle2", "POST", &handleToggle2);
  ResourceNode * nodeToggle3 = new ResourceNode("/toggle3", "POST", &handleToggle3);
  ResourceNode * nodeToggle4 = new ResourceNode("/toggle4", "POST", &handleToggle4);
  ResourceNode * nodeToggle5 = new ResourceNode("/toggle5", "POST", &handleToggle5);

  // Add the root node to the server
  secureServer->registerNode(nodeRoot);
  // Add the 404 not found node to the server.
  secureServer->setDefaultNode(node404);
  // Add Terminal node
  secureServer->registerNode(nodeTerminal);

  secureServer->registerNode(nodeToggle1);
  secureServer->registerNode(nodeToggle2);
  secureServer->registerNode(nodeToggle3);
  secureServer->registerNode(nodeToggle4);
  secureServer->registerNode(nodeToggle5);

  Serial.println("Starting server...");
  secureServer->start();
  if (secureServer->isRunning()) {
    Serial.println("Server ready.");
  }
}

void loop() {
  // This call will let the server do its work
  secureServer->loop();

  // Other code would go here...
  delay(1);
}

void handleRoot(HTTPRequest * req, HTTPResponse * res) {
  // Status code is 200 OK by default.
  // We want to deliver a simple HTML page, so we send a corresponding content type:
  res->setHeader("Content-Type", "text/html");

  // The response implements the Print interface, so you can use it just like
  // you would write to Serial etc.
  res->println("<!DOCTYPE html>");
  res->println("<html>");
  res->println("<head><title>Hello World!</title></head>");
  res->println("<body>");
  res->println("<h1>Hello World!</h1>");
  res->print("<p>Your server is running for ");
  // A bit of dynamic data: Show the uptime
  res->print((int)(millis()/1000), DEC);
  res->println(" seconds.</p>");
  res->println("</body>");
  res->println("</html>");
}

void handleTerminal(HTTPRequest * req, HTTPResponse * res) {
 // Status code is 200 OK by default.
  // We want to deliver a simple HTML page, so we send a corresponding content type:
  res->setHeader("Content-Type", "text/html");

  // The response implements the Print interface, so you can use it just like
  // you would write to Serial etc.
  res->println("<!DOCTYPE html>");
res->println("<html lang=\"en\">");
res->println("<head>");
res->println("    <meta charset=\"UTF-8\">");
res->println("    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
res->println("    <title>Xterm.js Demo</title>");
res->println("    <link rel=\"stylesheet\" href=\"https://unpkg.com/xterm/css/xterm.css\" />");
res->println("    <script src=\"https://unpkg.com/xterm/lib/xterm.js\"></script>");
res->println("</head>");
res->println("<body>");
res->println("    <div id=\"terminal\"></div>");
res->println("<iframe name='dummyframe' id='dummyframe' style='display: none;'></iframe>");
res->println("<form action='/toggle1' method='post' target='dummyframe'>Toggle HP 1<input type='submit'></form>");
res->println("<form action='/toggle2' method='post' target='dummyframe'>Toggle HP 2<input type='submit'></form>");
res->println("<form action='/toggle3' method='post' target='dummyframe'>Toggle Lenvo 1<input type='submit'></form>");
res->println("<form action='/toggle4' method='post' target='dummyframe'>Toggle Acer 1<input type='submit'></form>");
res->println("<form action='/toggle5' method='post' target='dummyframe'>Toggle Acer 2<input type='submit'></form>");

res->println("    <script>");

res->println("    const term = new Terminal();");
res->println("    const terminalContainer = document.getElementById('terminal');");
res->println("    term.open(terminalContainer);");

res->println("    term.write('Welcome to Xterm.js Demo\\n');");

res->println("    </script>");
res->println("</body>");
res->println("</html>");

};

void handleToggle1(HTTPRequest * req, HTTPResponse * res) {
  res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
  res->println("<!DOCTYPE html>");
  digitalWrite(LAPTOP_HP_1, HIGH);
  delay(1000);
  digitalWrite(LAPTOP_HP_1, LOW);
}
void handleToggle2(HTTPRequest * req, HTTPResponse * res) {
  res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
  res->println("<!DOCTYPE html>");
  digitalWrite(LAPTOP_HP_2, HIGH);
  delay(1000);
  digitalWrite(LAPTOP_HP_2, LOW);
}
void handleToggle3(HTTPRequest * req, HTTPResponse * res) {
  res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
  res->println("<!DOCTYPE html>");
  digitalWrite(LAPTOP_LENOVO_1, HIGH);
  delay(1000);
  digitalWrite(LAPTOP_LENOVO_1, LOW);
}
void handleToggle4(HTTPRequest * req, HTTPResponse * res) {
  res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
  res->println("<!DOCTYPE html>");
  digitalWrite(LAPTOP_ACER_1, HIGH);
  delay(1000);
  digitalWrite(LAPTOP_ACER_1, LOW);
}
void handleToggle5(HTTPRequest * req, HTTPResponse * res) {
  res->setStatusCode(200);
  res->setHeader("Content-Type", "text/html");
  res->println("<!DOCTYPE html>");
  digitalWrite(LAPTOP_ACER_2, HIGH);
  delay(1000);
  digitalWrite(LAPTOP_ACER_2, LOW);
}

void handle404(HTTPRequest * req, HTTPResponse * res) {
  // Discard request body, if we received any
  // We do this, as this is the default node and may also server POST/PUT requests
  req->discardRequestBody();

  // Set the response status
  res->setStatusCode(404);
  res->setStatusText("Not Found");

  // Set content type of the response
  res->setHeader("Content-Type", "text/html");

  // Write a tiny HTTP page
  res->println("<!DOCTYPE html>");
  res->println("<html>");
  res->println("<head><title>Not Found</title></head>");
  res->println("<body><h1>404 Not Found</h1><p>The requested resource was not found on this server.</p></body>");
  res->println("</html>");
}

