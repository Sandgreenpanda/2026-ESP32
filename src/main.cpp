#include "Arduino.h"
#include "HardwareSerial.h"
#include "WString.h"
#include "esp32-hal-ledc.h"
#include "esp32-hal.h"
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <WiFi.h>
#include <WiFiMulti.h>

#include <functional>
#include <libssh/libssh.h>
#include <lwip/sockets.h>
#include <sha/sha_parallel_engine.h>
#include <ssh_functions.h>

#include <FS.h>
#include <SD.h>
#include <SPI.h>
#define SD_MISO 16
#define SD_SCK 17
#define SD_MOSI 18
#define SD_CS 19

#define LAPTOP_HP_1 33
#define LAPTOP_HP_2 32
#define LAPTOP_HP_3 12
#define LAPTOP_LENOVO_1 27
#define LAPTOP_ACER_1 26
#define LAPTOP_ACER_2 25

#define PWM_FAN_1 23
#define TACH_FAN_1 22

// Max ssh clients. So fat this is for ne computer, so I may do different objs later.
// For now four is more than enough
#define MAX_CLIENTS 4

#define SSH_SINGLE_EXEC_TIMEOUT 10000 // 10s

boolean hp_1_conn = false;
boolean hp_2_conn = false;
boolean hp_3_conn = false;
boolean acer_1_conn = false;
boolean acer_2_conn = false;

boolean hp_1_status_req = false;
boolean hp_2_status_req = false;
boolean hp_3_status_req = false;
boolean acer_1_status_req = false;
boolean acer_2_status_req = false;

// Spi for the sd card file system
SPIClass sdSPI(VSPI);

// The HTTPS Server comes in a separate namespace. For easier use, include it here.
using namespace httpsserver;

WiFiMulti wifiMulti;

// Fan settings
const int PWM_FREQ = 25000; // 25 kHz frequency for computer fans
const int PWM_RESOLUTION = 8;
volatile unsigned long tachPulseCount = 0;
unsigned long lastTachTime = 0;
const unsigned long TACH_SAMPLE_TIME = 1000; // Sample period in milliseconds

// Interrupt service routine for tachometer
void IRAM_ATTR tachISR() {
    tachPulseCount = tachPulseCount + 1;
}

SSLCert *cert;
HTTPSServer *secureServer;

// Declare some handler functions for the various URLs on the server
void handleRoot(HTTPRequest *req, HTTPResponse *res);
void handle404(HTTPRequest *req, HTTPResponse *res);
void handleTerminal(HTTPRequest *req, HTTPResponse *res);
void handleTerminalPost(HTTPRequest *req, HTTPResponse *res);
void handleToggle1(HTTPRequest *req, HTTPResponse *res);
void handleSSHStatus1(HTTPRequest *req, HTTPResponse *res);
void handleToggle2(HTTPRequest *req, HTTPResponse *res);
void handleToggle3(HTTPRequest *req, HTTPResponse *res);
void handleToggle4(HTTPRequest *req, HTTPResponse *res);
void handleToggle5(HTTPRequest *req, HTTPResponse *res);
void handleToggle6(HTTPRequest *req, HTTPResponse *res);
void handleFan(HTTPRequest *req, HTTPResponse *res);
void handleStyle(HTTPRequest *req, HTTPResponse *res);
void handleTerminalUpdate(HTTPRequest *req, HTTPResponse *res);
void handleComputers(HTTPRequest *req, HTTPResponse *res);
void handleAdmin(HTTPRequest *req, HTTPResponse *res);
void handleLogIn(HTTPRequest *req, HTTPResponse *res);
void handleSSHpage(HTTPRequest *req, HTTPResponse *res);

void middlewareAuth(HTTPRequest *req, HTTPResponse *res, std::function<void()> next);

// List of clients that signed up for sse
std::vector<httpsserver::HTTPResponse *> sseClients;
// std::vector<int> sseClients;

const unsigned int configSTACK = 51200;

volatile devState_t devState;
volatile bool gotIpAddr, gotIp6Addr;
volatile bool wifiPhyConnected;

String ssh_command = "";
String shh_output_string = "";
String shh_output_send_temp_old = "";

String shh_output_send = "";
String shh_output_send_temp = "";

String PASSWORD = "OYW7]X}7:)[Z2T;l58-P6(P6+{21t0tF"; // This is NOT my password.
// This is a fallback for if the sd card does not load
// There is no way to not commit the fallback to github, by nature of it being a fallback
// I can't initialise it as an empty string, because in the case the SD card does not load,
// an empty string is not secure.

// SSH handler class
class SSHHandler : public WebsocketHandler {
  public:
    // This method is called by the webserver to instantiate a new handler for each
    // client that connects to the websocket endpoint
    static WebsocketHandler *create();

    // This method is called when a message arrives
    void onMessage(WebsocketInputStreambuf *input);

    // Handler function on connection close
    void onClose();
};

// Simple array to store the active clients:
SSHHandler *activeClients[MAX_CLIENTS];

class ssh_conn {
    // All my code :)
  private:
    char *host;
    char *user;
    char *password;
    ssh_channel channel;
    ssh_session session;

  public:
    ssh_conn(char *host, char *user, char *password) {
        this->host = host;
        this->user = user;
        this->password = password;
    }

    void brutal_exception() {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);
        ssh_finalize();
    }

    ssh_channel connect() {
        // Except for this bit ;)

        int rc;
        session = connect_ssh(password, host, user, 0);
        // session = connect_ssh("password", "test.rebex.net", "demo", 0);

        if (session == NULL) {
            ssh_finalize();
            return NULL;
        }

        channel = ssh_channel_new(session);

        rc = ssh_channel_open_session(channel);
        if (rc != SSH_OK) {
            Serial.println("Open Fail");
            this->brutal_exception();
            return NULL;
        }

        rc = ssh_channel_request_pty(channel);
        if (rc != SSH_OK) {
            Serial.println("Request Fail");
            this->brutal_exception();
            return NULL;
        }
        rc = ssh_channel_change_pty_size(channel, 100, 30);
        if (rc != SSH_OK) {
            Serial.println("Resize Fail");
            this->brutal_exception();
            return NULL;
        }
        rc = ssh_channel_request_shell(channel);
        if (rc != SSH_OK) {
            Serial.println("Shell Fail");
            this->brutal_exception();
            return NULL;
        }
        return channel;
    }

    int read() {
        char buffer[256];
        int rbytes;

        shh_output_send_temp = "";
        rbytes = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 0);

        if (rbytes > 0) {

            std::string msg(buffer, rbytes);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (activeClients[i] != nullptr) {
                    // Serial.print("THIS +");
                    Serial.println(msg.c_str());
                    activeClients[i]->send(msg, WebsocketHandler::SEND_TYPE_TEXT);
                }
            }
        };
        return rbytes;
    }

    void write(String command) {
        Serial.println("ssh cmd received:");
        Serial.println(command);
        ssh_channel_write(channel, command.c_str(), command.length());
        Serial.println("ssh command processing finished");
    }

    void keep_alive() {
        ssh_send_ignore(session, "keepalive");
    }

    String exec_cmd(String command) {
        ssh_channel channel_oneoff = NULL;
        int rc;
        int nbytes;
        char buffer[256];
        String ssh_output = "";

        channel_oneoff = ssh_channel_new(session);
        if (channel_oneoff == NULL)
            return "-1"; // SSH ERROR

        rc = ssh_channel_open_session(channel_oneoff);
        if (rc != SSH_OK) {
            ssh_channel_free(channel_oneoff);
            return String(rc);
        }

        rc = ssh_channel_request_exec(channel_oneoff, command.c_str());
        if (rc != SSH_OK) {
            ssh_channel_close(channel_oneoff);
            ssh_channel_free(channel_oneoff);
            return String(rc);
        }

        unsigned long startTime = millis();

        while (ssh_channel_is_open(channel_oneoff) && !ssh_channel_is_eof(channel_oneoff)) {
            nbytes = ssh_channel_read_nonblocking(channel_oneoff, buffer, sizeof(buffer), 0);

            if (nbytes > 0) {
                ssh_output += String(buffer, nbytes);
                startTime = millis();
            } else if (nbytes == SSH_ERROR) {
                return "-1"; // SSH ERROR
            }

            if (millis() - startTime > SSH_SINGLE_EXEC_TIMEOUT) {
                Serial.println("Command timed out waiting for EOF!");
                break;
            }
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }

        nbytes = ssh_channel_read_nonblocking(channel_oneoff, buffer, sizeof(buffer), 0);

        if (nbytes > 0) {
            ssh_output += String(buffer, nbytes);
            startTime = millis();
        } else if (nbytes == SSH_ERROR) {
            return "-1"; // SSH ERROR
        }

        ssh_channel_close(channel_oneoff);
        ssh_channel_free(channel_oneoff);

        return ssh_output;
    }

    void disconnect() {
        ssh_disconnect(session);
        ssh_free(session);
        ssh_finalize();
    }
};

int ex_main() {
    Serial.println("Exec main begin");
    ssh_session session = NULL;
    ssh_channel channel = NULL;

    int nbytes;
    int nwritten;

    int writeBufferSize;

    int lastHandleKeepAlive = millis();

    // Temporarily using test server
    // session = connect_ssh("Home1918", "10.47.1.39", "alext", 0);

    ssh_conn hp_1_session("192.168.1.25", "alext", "Home1918");
    hp_1_session.connect();

    Serial.println("loop begin");
    while (1) {

        // Read the ssh data and send it to the websocket clients
        if (hp_1_session.read() < 1) { // Returns rbytes, checks for no-bytes-transferred/error
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }

        // shh_output_send = shh_output_send_temp; // Sends the data for processing

        // This loop goes though ever client and attempts to update it with the new ssh data
        // If the client does not respond it is assumed disconnected and is removed
        // Commented out because SSE is deprecated, use websockets
        /*
        for (auto client = sseClients.begin(); client != sseClients.end();) { // Note there is no auto increment, this is done manually
            size_t bytesWritten = (*client)->print(shh_output_send_temp);
            // int bytesWritten = lwip_send((*client), shh_output_send_temp.c_str(), shh_output_send_temp.length(), MSG_DONTWAIT);

            if (bytesWritten <= 0 && shh_output_send_temp != "") { // Less than 0 is error code, and 0 is no bytes written.
                                                                   // We also make sure we actually were writing bytes to prevent false positives

                // Removes the item, as the client has disconnected. Sets the iterator to the next valid client
                delete *client;
                client = sseClients.erase(client);
            } else {
                client++;
            }
        }
        */

        if (ssh_command != "") {
            // Write the websocket data to ssh
            hp_1_session.write(ssh_command);
            ssh_command = ""; // Reset command to prevent infinite loop

        } else if (hp_1_status_req) {
            hp_1_conn = (hp_1_session.exec_cmd("echo alive") == "alive");

            if (!hp_1_conn) {
                Serial.println("HP1 Disconnect! Reconnecting...");
                // TODO: Reconnect logic for turn on
                hp_1_session.disconnect();
                hp_1_session.connect();
            }
            hp_1_status_req = false;
        } else {
            if (millis() - lastHandleKeepAlive >= 10000) { // Every 10 seconds
                Serial.println("Keep Alive Check");
                hp_1_session.keep_alive();
                lastHandleKeepAlive = millis();
            }
        }
    }
    hp_1_session.disconnect();

    return 0;
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

    // Setup fans
    // First pin is declared to use chanel 0
    ledcSetup(0, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PWM_FAN_1, 0);
    ledcWrite(0, 0); // Set initial fan speed to zero

    // Configure tachometer pin with interrupt
    pinMode(TACH_FAN_1, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TACH_FAN_1), tachISR, FALLING);

    // Initialize timing for fan tach
    lastTachTime = millis();

    WiFi.disconnect(true);

    // Max wifi strength
    // WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.setSleep(false);

    wifiMulti.addAP("WC Devices", "0jebr9yrxh");
    wifiMulti.addAP("SPARK-UMRK2N", "NKUWEW7ZZV");

    // ESP32 is 2.4GHZ only
    // wifiMulti.addAP("SPARK-UMRK2N-5G", "NKUWEW7ZZV");

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

    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, sdSPI)) {
        Serial.println("SD mount failed");
    } else {
        Serial.println("SD mount successful");
    }

    // Real password is initialed from the SD
    PASSWORD = SD.open("/crypto/password.txt", FILE_READ).readString();

    // Initialise the custom wifi event handler, which allows wait_for_wifi_exec to know when the ipv4 and v6 addresses have been set.
    esp_netif_init();
    esp_event_loop_create_default();
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_cb, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, event_cb, NULL, NULL);

    // Stack size needs to be larger, so continue in a new task.

    xTaskCreatePinnedToCore(controlTask, "ctl", configSTACK, NULL, (tskIDLE_PRIORITY + 3), NULL, 0);

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
    ResourceNode *nodeAdmin = new ResourceNode("/admin", "GET", &handleAdmin);
    ResourceNode *nodeLogIn = new ResourceNode("/admin", "POST", &handleLogIn);

    ResourceNode *nodeHandleFan = new ResourceNode("/fan", "POST", &handleFan);
    ResourceNode *nodeHandleStyle = new ResourceNode("/style.css", "GET", &handleStyle);
    ResourceNode *nodeHandleComputers = new ResourceNode("/computers", "GET", &handleComputers);

    ResourceNode *nodeHandleTerminalUpdate = new ResourceNode("/update", "GET", &handleTerminalUpdate);
    ResourceNode *nodehandleSSHpage = new ResourceNode("/sshpage", "GET", &handleSSHpage);
    WebsocketNode *sshNode = new WebsocketNode("/ssh", &SSHHandler::create);
    ResourceNode *nodeSSHStatus1 = new ResourceNode("/SSH1Status", "POST", &handleSSHStatus1);

    // Adding the node to the server works in the same way as for all other nodes
    secureServer->registerNode(sshNode);
    secureServer->registerNode(nodehandleSSHpage);

    // Add the root node to the server
    secureServer->registerNode(nodeRoot);
    // Add the 404 not found node to the server.
    secureServer->setDefaultNode(node404);
    // Add Terminal node
    secureServer->registerNode(nodeTerminal);
    secureServer->registerNode(nodeTerminalPost);
    secureServer->registerNode(nodeSSHStatus1);
    secureServer->registerNode(nodeToggle1);
    secureServer->registerNode(nodeToggle2);
    secureServer->registerNode(nodeToggle3);
    secureServer->registerNode(nodeToggle4);
    secureServer->registerNode(nodeToggle5);
    secureServer->registerNode(nodeToggle6);
    secureServer->registerNode(nodeHandleFan);
    secureServer->registerNode(nodeHandleComputers);

    secureServer->registerNode(nodeAdmin);
    secureServer->registerNode(nodeLogIn);

    secureServer->registerNode(nodeHandleStyle);

    secureServer->registerNode(nodeHandleTerminalUpdate);

    secureServer->addMiddleware(&middlewareAuth);

    Serial.println("Starting server...");
    secureServer->start();
    if (secureServer->isRunning()) {
        Serial.println("Server ready.");
    }
}

void loop() {
    secureServer->loop();
    delay(1);
    if (millis() - lastTachTime >= TACH_SAMPLE_TIME) {
        // Calculate RPM (2 pulses per revolution for most fans)
        unsigned long rpm = (tachPulseCount * 60000) / (TACH_SAMPLE_TIME * 2);
        Serial.println(rpm);
        tachPulseCount = 0;
        lastTachTime = millis();
    };
    // Print CPU temps
    /*
    if (ssh_command == "") {
        Serial.println(shh_output_string);
        shh_output_string = "";
        ssh_command = "sensors | grep -E \"Core [0-9]\" | awk \'{print $3}\'";
    };
    */
}

void middlewareAuth(HTTPRequest *req, HTTPResponse *res, std::function<void()> next) {
    String user_password_raw = req->getHeader("Cookie").c_str();
    String req_str = req->getRequestString().c_str();
    Serial.println(req_str);
    Serial.println(user_password_raw);

    // Extract the password from the cookie
    String user_password = user_password_raw.substring(user_password_raw.lastIndexOf("=") + 1, user_password_raw.length());
    Serial.println(user_password);

    if (user_password == PASSWORD || req_str == "/style.css" || req_str == "/admin" || req_str == "/update") {
        if (user_password == PASSWORD && req_str == "/admin") {
            res->setHeader("Content-Type", "text/html");
            res->println(SD.open("/templates/log_out.html", FILE_READ).readString());
        } else {
            next();
        }
    } else {
        res->setStatusCode(404);
        res->setHeader("Content-Type", "text/html");
        res->println(SD.open("/templates/no_auth.html", FILE_READ).readString());
    }
}

void handleRoot(HTTPRequest *req, HTTPResponse *res) {
    res->setHeader("Content-Type", "text/html");
    String file = SD.open("/templates/home.html", FILE_READ).readString();
    file.replace("%TIME%", (String)((int)(millis() / 1000)));
    res->println(file);
}

void handleStyle(HTTPRequest *req, HTTPResponse *res) {
    res->setHeader("Content-Type", "text/css");
    res->println(SD.open("/static/style.css", FILE_READ).readString());
};

void handleTerminal(HTTPRequest *req, HTTPResponse *res) {
    // Status code is 200 OK by default.
    // We want to deliver a simple HTML page, so we send a corresponding content type:
    res->setHeader("Content-Type", "text/html");

    res->println(SD.open("/templates/terminal.html", FILE_READ).readString());
};

void handleComputers(HTTPRequest *req, HTTPResponse *res) {
    res->setHeader("Content-Type", "text/html");
    res->println(SD.open("/templates/computers.html", FILE_READ).readString());
};

// Websockets
WebsocketHandler *SSHHandler::create() {
    Serial.println("Creating new chat client!");
    SSHHandler *handler = new SSHHandler();

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (activeClients[i] == nullptr) {
            activeClients[i] = handler;
            break;
        }
    }
    return handler;
}

// When the websocket is closing, we remove the client from the array
void SSHHandler::onClose() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (activeClients[i] == this) {
            activeClients[i] = nullptr;
        }
    }
}

void SSHHandler::onMessage(WebsocketInputStreambuf *inbuf) {
    // Get the input message
    // Dont know how this works, but I got it from the default websocket chat example
    // Might figure it out later
    std::ostringstream ss;
    std::string msg;
    ss << inbuf;
    msg = ss.str();
    String ssh_msg = msg.c_str();

    // Send the ssh output to the client

    Serial.println(ssh_msg);
    ssh_command = ssh_msg;

    /*
    this->send(shh_output_string.c_str(), SEND_TYPE_TEXT);
    shh_output_string = "";
    */
}

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

    // res->Save(); // Pointers no longer disappear
    //  Allows for SSE

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

void handleFan(HTTPRequest *req, HTTPResponse *res) {
    res->setHeader("Content-Type", "text/plain");
    byte buffer[256];

    String fanSpeedInput = "";
    while (!(req->requestComplete())) {
        size_t s = req->readBytes(buffer, 256);
        fanSpeedInput += String(buffer, s);
    }
    ledcWrite(0, fanSpeedInput.substring(1, fanSpeedInput.length() - 1).toInt());
    // ledcWrite(0, fanSpeedInput.substring(6).toInt()); // Substring cuts off: speed=
    // Serial.println(fanSpeedInput.substring(6).toInt());
}

// TODO: Update this to use one rout
void handleToggle1(HTTPRequest *req, HTTPResponse *res) {
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

void handleSSHStatus1(HTTPRequest *req, HTTPResponse *res) {
    hp_1_status_req = true;
    res->setHeader("Content-Type", "application/json");
    while (hp_1_status_req) {
        delay(1);
    }
    res->println(hp_1_conn);
}

void handleAdmin(HTTPRequest *req, HTTPResponse *res) {
    res->setHeader("Content-Type", "text/html");
    res->println(SD.open("/templates/admin.html", FILE_READ).readString());
};

void handleSSHpage(HTTPRequest *req, HTTPResponse *res) {
    res->setHeader("Content-Type", "text/html");
    res->println(SD.open("/templates/websocket.html", FILE_READ).readString());
};

void handleLogIn(HTTPRequest *req, HTTPResponse *res) {
    byte buffer[256];

    String data = "";
    while (!(req->requestComplete())) {
        size_t s = req->readBytes(buffer, 256);
        data += String(buffer, s);
    }
    Serial.println(data);

    String Session = data; //.substring(11, data.length() - 1);
    Serial.println(Session);

    res->setHeader("Content-Type", "text/html");
    String Cookie = "session=" + Session + "; Path=/; SameSite=Strict; Secure";
    res->setHeader("Set-Cookie", Cookie.c_str());
    res->setHeader("Content-Type", "text/html");
    res->println(SD.open("/templates/log_out.html", FILE_READ).readString());
};

void handle404(HTTPRequest *req, HTTPResponse *res) {
    // Discard request body, if we received any
    // We do this, as this is the default node and may also server POST/PUT requests
    req->discardRequestBody();

    // Set the response status
    res->setStatusCode(404);
    res->setStatusText("Not Found");

    // Set content type of the response
    res->setHeader("Content-Type", "text/html");

    // Write a tiny HTML page
    res->println("<!DOCTYPE html>");
    res->println("<html>");
    res->println("<head><title>Not Found</title></head>");
    res->println("<body><h1>404 Not Found</h1><p>The requested resource was not found on this server.</p></body>");
    res->println("</html>");
}