#include <WiFi.h>
#include <WiFiMulti.h>
#include <libssh/libssh.h>
#include <ssh_functions.h>

WiFiMulti wifiMulti;

const unsigned int configSTACK = 51200;

volatile devState_t devState;
volatile bool gotIpAddr, gotIp6Addr;
volatile bool wifiPhyConnected;

bool comunicating = false;
String shh_output_string = "";

int ex_main() {
    ssh_session session = NULL;
    ssh_channel channel = NULL;
    char buffer[256];
    int rbytes;
    int rc;

    session = connect_ssh("Home1918", "192.168.1.71", "alext", 0);
    if (session == NULL) {
        ssh_finalize();
        return 1;
    }
    channel = ssh_channel_new(session);
    if (channel == NULL) {
        ssh_disconnect(session);
        ssh_free(session);
        ssh_finalize();
        return 1;
    }

    rc = ssh_channel_open_session(channel);
    if (rc < 0) {
        goto failed;
    }

    rc = ssh_channel_request_exec(channel, "ls");
    if (rc < 0) {
        goto failed;
    }

    // Reads the ssh output
    shh_output_string = "";
    rbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
    do {
        shh_output_string += String(buffer, rbytes);
        rbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
    } while (rbytes > 0);

    // Prints the ssh output
    Serial.println(shh_output_string);

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
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
    WiFi.disconnect(true);
    wifiMulti.addAP("WC Devices", "iujonmhmjm");
    wifiMulti.addAP("SPARK-UMRK2N", "NKUWEW7ZZV");
    wifiMulti.addAP("SPARK-UMRK2N-5G", "NKUWEW7ZZV");

    devState = STATE_NEW;

    // Use the expected blocking I/O behavior.
    // setvbuf(stdin, NULL, _IONBF, 0);
    // setvbuf(stdout, NULL, _IONBF, 0);
    Serial.begin(115200);

    // Initialise the custom wifi event handler, which allows wait_for_wifi_exec to know when the ipv4 and v6 addresses have been set.
    esp_netif_init();
    esp_event_loop_create_default();
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_cb, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, event_cb, NULL, NULL);

    // Stack size needs to be larger, so continue in a new task.
    xTaskCreatePinnedToCore(controlTask, "ctl", configSTACK, NULL, (tskIDLE_PRIORITY + 3), NULL, portNUM_PROCESSORS - 1);
}

void loop() {
}