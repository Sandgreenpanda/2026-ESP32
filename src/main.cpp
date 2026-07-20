#include "HardwareSerial.h"
#include "esp32-hal.h"
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <libssh/libssh.h>
#include <sha/sha_parallel_engine.h>
#include <ssh_functions.h>

#include <lwip/sockets.h>

#define LAPTOP_HP_1 33
#define LAPTOP_HP_2 32
#define LAPTOP_HP_3 12
#define LAPTOP_LENOVO_1 27
#define LAPTOP_ACER_1 26
#define LAPTOP_ACER_2 25

// The HTTPS Server comes in a separate namespace. For easier use, include it here.
using namespace httpsserver;

WiFiMulti wifiMulti;

SSLCert *cert;
HTTPSServer *secureServer;

// Declare some handler functions for the various URLs on the server
void handleRoot(HTTPRequest *req, HTTPResponse *res);
void handle404(HTTPRequest *req, HTTPResponse *res);
void handleTerminal(HTTPRequest *req, HTTPResponse *res);
void handleTerminalPost(HTTPRequest *req, HTTPResponse *res);
void handleToggle1(HTTPRequest *req, HTTPResponse *res);
void handleToggle2(HTTPRequest *req, HTTPResponse *res);
void handleToggle3(HTTPRequest *req, HTTPResponse *res);
void handleToggle4(HTTPRequest *req, HTTPResponse *res);
void handleToggle5(HTTPRequest *req, HTTPResponse *res);
void handleToggle6(HTTPRequest *req, HTTPResponse *res);
void handleTerminalUpdate(HTTPRequest *req, HTTPResponse *res);

// List of clients that signed up for sse
std::vector<httpsserver::HTTPResponse*> sseClients;
//std::vector<int> sseClients;

const unsigned int configSTACK = 51200;

volatile devState_t devState;
volatile bool gotIpAddr, gotIp6Addr;
volatile bool wifiPhyConnected;

String ssh_command = "";
String shh_output_string = "";

String shh_output_send = "";
String shh_output_send_temp = "";

int ex_main() {
    ssh_session session = NULL;
    ssh_channel channel = NULL;
    char buffer[256];
    int rbytes;
    int rc;

    int nbytes;
    int nwritten;

    int writeBufferSize;

    int lastHandleKeepAlive = millis();

    session = connect_ssh("Home1918", "192.168.1.222", "alext", 0);
    if (session == NULL) {
        ssh_finalize();
        return 1;
    }

    channel = ssh_channel_new(session);

    rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        Serial.println("Open Fail");
        goto failed;
    }

    rc = ssh_channel_request_pty(channel);
    if (rc != SSH_OK) {
        Serial.println("Request Fail");
        goto failed;
    }
    rc = ssh_channel_change_pty_size(channel, 100, 30);
    if (rc != SSH_OK) {
        Serial.println("Resize Fail");
        goto failed;
    }
    rc = ssh_channel_request_shell(channel);
    if (rc != SSH_OK) {
        goto failed;
        Serial.println("Shell Fail");
    }
    while (1) {
        // nbytes = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 0);
        //   nbytes = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 0);

        // if (nbytes > 0) {
        //    shh_output_string = String(buffer, nbytes);
        //   Serial.println(shh_output_string);
        // }

        rbytes = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 0);

        if (rbytes > 0) {
            shh_output_send_temp = String(buffer, rbytes);
        }

        do {
            Serial.println("response loop");
            shh_output_send_temp += String(buffer, rbytes);
            rbytes = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 0);
            Serial.println(rbytes);
        } while (rbytes > 0);

        Serial.println(shh_output_send_temp);
        // shh_output_send = shh_output_send_temp; // Sends the data for processing

        // This loop goes though ever client and attempts to update it with the new ssh data
        // If the client does not respond it is assumed disconnected and is removed
        for (auto client = sseClients.begin(); client != sseClients.end();) { // Note there is no auto increment, this is done manually
            size_t bytesWritten = (*client)->print(shh_output_send_temp);
            //int bytesWritten = lwip_send((*client), shh_output_send_temp.c_str(), shh_output_send_temp.length(), MSG_DONTWAIT);

            if (bytesWritten <= 0 && shh_output_send_temp != "") { // Less than 0 is error code, and 0 is no bytes written.
                                                                   // We also make sure we actually were writing bytes to prevent false positives

                // Removes the item, as the client has disconnected. Sets the iterator to the next valid client
                delete *client;
                client = sseClients.erase(client);
            } else {
                client++;
            }
        }

        if (ssh_command != "") {
            Serial.println("ssh cmd received");
            Serial.println(ssh_command);
            // channel = ssh_channel_new(session);

            // Better ssh start:

            // Put the command bytes the format ssh write wants

            ssh_command += "\n";

            ssh_channel_write(channel, ssh_command.c_str(), ssh_command.length());

            /*

            rc = ssh_channel_open_session(channel);
            //if (rc < 0) {
            //    goto failed;
            //}
            rc = ssh_channel_request_exec(channel, ssh_command.c_str());
            // if (rc < 0) {
            //     goto failed;
            //  }
            Serial.println(rc);
            // Reads the ssh output
            shh_output_string = "";
            rbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
            Serial.println("response 1");


            */

            // Prints the ssh output
            //  Serial.println(shh_output_string);
            //   shh_output_string = "";
            rbytes = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 0);

            if (rbytes > 0) {
                shh_output_string = String(buffer, rbytes);
            }
            do {
                Serial.println("response loop");
                shh_output_string += String(buffer, rbytes);
                rbytes = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 0);
                Serial.println(rbytes);
            } while (rbytes > 0);

            Serial.println(shh_output_string);

            ssh_command = ""; // Reset command to prevent infinite loop

            //  ssh_channel_send_eof(channel);
            // ssh_channel_close(channel);
            // ssh_channel_free(channel);
            Serial.println("ssh command processing finished");

        } else {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            if (millis() - lastHandleKeepAlive >= 10000) { // Every 10 seconds
                Serial.println("Keep Alive Check");
                ssh_send_ignore(session, "keepalive");
                lastHandleKeepAlive = millis();
            }
        }
    }

    ssh_disconnect(session);
    ssh_free(session);
    ssh_finalize();
    return 0;
failed:
    Serial.println("FAIL");
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    ssh_disconnect(session);
    ssh_free(session);
    ssh_finalize();
    return 1;
}

void controlTask(void *pvParameter) {
    wifiMulti.run();
    Serial.println("Wifi run finished");
    wait_for_wifi_exec(ex_main);
}

void setup() {

    digitalWrite(LAPTOP_HP_1, LOW);
    digitalWrite(LAPTOP_HP_2, LOW);
    digitalWrite(LAPTOP_HP_3, LOW);
    digitalWrite(LAPTOP_LENOVO_1, LOW);
    digitalWrite(LAPTOP_ACER_1, LOW);
    digitalWrite(LAPTOP_ACER_2, LOW);
    pinMode(LAPTOP_HP_1, OUTPUT);
    pinMode(LAPTOP_HP_2, OUTPUT);
    pinMode(LAPTOP_HP_3, OUTPUT);
    pinMode(LAPTOP_LENOVO_1, OUTPUT);
    pinMode(LAPTOP_ACER_1, OUTPUT);
    pinMode(LAPTOP_ACER_2, OUTPUT);

    WiFi.disconnect(true);
    wifiMulti.addAP("WC Devices", "iujonmhmjm");
    wifiMulti.addAP("SPARK-UMRK2N", "NKUWEW7ZZV");
    wifiMulti.addAP("SPARK-UMRK2N-5G", "NKUWEW7ZZV");

    devState = STATE_NEW;

    Serial.begin(115200);

    Serial.println("Creating a new self-signed certificate.");
    Serial.println("This may take up to a minute, so be patient ;-)");

    cert = new SSLCert();

    int createCertResult = createSelfSignedCert(
        *cert,
        KEYSIZE_1024,
        "CN=10.47.6.92,O=Sandgreenpanda,C=DE",
        "20190101000000",
        "20300101000000");

    // Now check if creating that worked
    if (createCertResult != 0) {
        Serial.printf("Cerating certificate failed. Error Code = 0x%02X, check SSLCert.hpp for details", createCertResult);
        while (true)
            delay(500);
    }
    Serial.println("Creating the certificate was successful");

    // Initialise the custom wifi event handler, which allows wait_for_wifi_exec to know when the ipv4 and v6 addresses have been set.
    esp_netif_init();
    esp_event_loop_create_default();
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_cb, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, event_cb, NULL, NULL);

    // Stack size needs to be larger, so continue in a new task.

    xTaskCreatePinnedToCore(controlTask, "ctl", configSTACK, NULL, (tskIDLE_PRIORITY + 3), NULL, portNUM_PROCESSORS - 1);

    secureServer = new HTTPSServer(cert);

    ResourceNode *nodeRoot = new ResourceNode("/", "GET", &handleRoot);
    ResourceNode *node404 = new ResourceNode("", "GET", &handle404);
    ResourceNode *nodeTerminal = new ResourceNode("/terminal", "GET", &handleTerminal);
    ResourceNode *nodeTerminalPost = new ResourceNode("/terminal_post", "POST", &handleTerminalPost);
    ResourceNode *nodeToggle1 = new ResourceNode("/toggle1", "POST", &handleToggle1);
    ResourceNode *nodeToggle2 = new ResourceNode("/toggle2", "POST", &handleToggle2);
    ResourceNode *nodeToggle3 = new ResourceNode("/toggle3", "POST", &handleToggle3);
    ResourceNode *nodeToggle4 = new ResourceNode("/toggle4", "POST", &handleToggle4);
    ResourceNode *nodeToggle5 = new ResourceNode("/toggle5", "POST", &handleToggle5);
    ResourceNode *nodeToggle6 = new ResourceNode("/toggle6", "POST", &handleToggle6);

    ResourceNode *nodeHandleTerminalUpdate = new ResourceNode("/update", "GET", &handleTerminalUpdate);

    // Add the root node to the server
    secureServer->registerNode(nodeRoot);
    // Add the 404 not found node to the server.
    secureServer->setDefaultNode(node404);
    // Add Terminal node
    secureServer->registerNode(nodeTerminal);
    secureServer->registerNode(nodeTerminalPost);

    secureServer->registerNode(nodeToggle1);
    secureServer->registerNode(nodeToggle2);
    secureServer->registerNode(nodeToggle3);
    secureServer->registerNode(nodeToggle4);
    secureServer->registerNode(nodeToggle5);
    secureServer->registerNode(nodeToggle6);

    secureServer->registerNode(nodeHandleTerminalUpdate);

    Serial.println("Starting server...");
    secureServer->start();
    if (secureServer->isRunning()) {
        Serial.println("Server ready.");
    }
}

void loop() {
    secureServer->loop();
    delay(1);
}

void handleRoot(HTTPRequest *req, HTTPResponse *res) {
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
    res->print((int)(millis() / 1000), DEC);
    res->println(" seconds.</p>");
    res->println("</body>");
    res->println("</html>");
}

void handleTerminal(HTTPRequest *req, HTTPResponse *res) {
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
    res->println("<form action='/toggle6' method='post' target='dummyframe'>Toggle HP 3<input type='submit'></form>");

    res->println("    <script>");

    res->println("    const term = new Terminal({rows: 30,cols: 100});");
    res->println("    const terminalContainer = document.getElementById('terminal');");
    res->println("    term.open(terminalContainer);");

    res->println("    term.write('Welcome to Xterm.js Demo\\n\\r$ ');");

    res->println("let command = '';");

    res->println("term.onData(e => {if (e === '\\r') {term.write('\\n\\r');handleCommand(command.trim());command = '';} else if (e === '\\x7F') {if (command.length > 0) {term.write('\\b \\b');command = command.slice(0, -1);}} else {term.write(e);command += e;}});");

    res->println("function handleCommand(input) {fetch('/terminal_post', {method:'post',headers: {'Accept': 'application/json','Content-Type': 'application/json'},body: JSON.stringify(input)}).then(response=>response.text()).then(data=>{ term.write(data.trim().replaceAll('\\n', '\\r\\n'));console.log(data);term.write('\\r\\n$ ');})}");

    res->println("    </script>");
    res->println("</body>");
    res->println("</html>");
};

void handleTerminalUpdate(HTTPRequest *req, HTTPResponse *res) {
    // sse headers

   //  res->setChunkedTransferMode();
    res->setHeader("Content-Type", "text/event-stream");
    res->setHeader("Cache-Control", "no-cache");
    res->setHeader("Connection", "keep-alive");

    // This code adds res to the list of clients that get sse updates
    res->print(""); // Force submit the headers

   // int rawSocketFd = req->getClientStartData()->_socket; 
    sseClients.push_back(res); // res is a pointer.


    while(1) {
        vTaskDelay(pdMS_TO_TICKS(50));
    } 
    // A client is removed upon a bad res->print in the ssh loop

    // res->flush();

    // This code was bad and blocked the whole thread
    /*
    while (1) {
        if (shh_output_send != "") {
            // res->write((uint8_t *)shh_output_send.c_str(), shh_output_send.length());
            res->print(shh_output_send);
            shh_output_send = "";
        }
        delay(10); // Let the esp complete other tasks in this thread
    }
    */
}

void handleTerminalPost(HTTPRequest *req, HTTPResponse *res) {
    // The echo callback will return the request body as response body.

    // We use text/plain for the response
    res->setHeader("Content-Type", "text/plain");

    // Stream the incoming request body to the response body
    // Theoretically, this should work for every request size.
    byte buffer[256];
    // HTTPReqeust::requestComplete can be used to check whether the
    // body has been parsed completely.
    String shh_input_string = "";
    while (!(req->requestComplete())) {
        // HTTPRequest::readBytes provides access to the request body.
        // It requires a buffer, the max buffer length and it will return
        // the amount of bytes that have been written to the buffer.
        size_t s = req->readBytes(buffer, 256);

        // The response does not only implement the Print interface to
        // write character data to the response but also the write function
        // to write binary data to the response.
        shh_input_string += String(buffer, s);
        // res->write(buffer, s);
    }
    Serial.println(shh_input_string);
    ssh_command = shh_input_string.substring(1, shh_input_string.length() - 1);
    while (ssh_command != "") {
        delay(1);
    }
    res->println(shh_output_string);
    shh_output_string = "";
}

void handleToggle1(HTTPRequest *req, HTTPResponse *res) {
    ssh_command = "ls"; // Execute command

    Serial.println("1");
    digitalWrite(LAPTOP_HP_1, HIGH);
    delay(1000);
    digitalWrite(LAPTOP_HP_1, LOW);
}
void handleToggle2(HTTPRequest *req, HTTPResponse *res) {
    Serial.println("2");
    digitalWrite(LAPTOP_HP_2, HIGH);
    delay(1000);
    digitalWrite(LAPTOP_HP_2, LOW);
}
void handleToggle3(HTTPRequest *req, HTTPResponse *res) {
    Serial.println("3");
    digitalWrite(LAPTOP_LENOVO_1, HIGH);
    delay(1000);
    digitalWrite(LAPTOP_LENOVO_1, LOW);
}
void handleToggle4(HTTPRequest *req, HTTPResponse *res) {
    Serial.println("4");
    digitalWrite(LAPTOP_ACER_1, HIGH);
    delay(1000);
    digitalWrite(LAPTOP_ACER_1, LOW);
}
void handleToggle5(HTTPRequest *req, HTTPResponse *res) {
    Serial.println("5");
    digitalWrite(LAPTOP_ACER_2, HIGH);
    delay(1000);
    digitalWrite(LAPTOP_ACER_2, LOW);
}
void handleToggle6(HTTPRequest *req, HTTPResponse *res) {
    Serial.println("6");
    digitalWrite(LAPTOP_HP_3, HIGH);
    delay(1000);
    digitalWrite(LAPTOP_HP_3, LOW);
}

void handle404(HTTPRequest *req, HTTPResponse *res) {
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


// SD card test:

/*
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#define SD_MISO 2
#define SD_MOSI 15
#define SD_SCK 14
#define SD_CS 13
SPIClass sdSPI = SPIClass(HSPI);
void setup() {
    Serial.begin(115200);
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, sdSPI)) {
        Serial.println("SD mount failed");
    } else {
        Serial.println("SD mount successful");
    }
}
*/