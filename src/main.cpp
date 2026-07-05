#include "IPv6Address.h"
#include "SPIFFS.h"
#include "WiFi.h"
#include "driver/uart.h"
#include "esp_netif.h"
#include "esp_vfs_dev.h"
#include "libssh/priv.h"
#include "libssh_esp32.h"
#include "libssh_esp32_config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <arpa/inet.h>
#include <errno.h>
#include <libssh/libssh.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ssh_functions.h"

int ex_main();

const char *configSTASSID = "SPARK-UMRK2N";
const char *configSTAPSK = "NKUWEW7ZZV";
#define newDevState(s) (devState = s)

#define EX_CMD "exec"

#define WIFI_TIMEOUT_S 10
#define NET_WAIT_MS 100

const unsigned int configSTACK = 51200;

volatile bool wifiPhyConnected;

typedef enum {
    STATE_NEW,
    STATE_PHY_CONNECTED,
    STATE_WAIT_IPADDR,
    STATE_GOT_IPADDR,
    STATE_OTA_UPDATING,
    STATE_OTA_COMPLETE,
    STATE_LISTENING,
    STATE_TCP_DISCONNECTED
} devState_t;

static volatile devState_t devState;
static volatile bool gotIpAddr, gotIp6Addr;

bool comunicating = false;

String shh_output_string = "";


void event_cb(void *args, esp_event_base_t base, int32_t id, void *event_data) {
    switch (id) {
    case WIFI_EVENT_STA_START:
        Serial.print("% WiFi enabled with SSID=");
        Serial.println(configSTASSID);
        break;
    case WIFI_EVENT_STA_CONNECTED:
        Serial.println("% WiFi connected");
        wifiPhyConnected = true;
        if (devState < STATE_PHY_CONNECTED)
            newDevState(STATE_PHY_CONNECTED);
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        if (devState < STATE_WAIT_IPADDR)
            newDevState(STATE_NEW);
        if (wifiPhyConnected) {
            Serial.println("% WiFi disconnected");
            wifiPhyConnected = false;
        }
        WiFi.begin(configSTASSID, configSTAPSK);
        break;
    case IP_EVENT_GOT_IP6: {
        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        if (event->ip6_info.ip.addr[0] != htons(0xFE80) && !gotIp6Addr) {
            gotIp6Addr = true;
        }
        Serial.print("% IPv6 Address: ");

        Serial.println(IPv6Address(event->ip6_info.ip.addr));
    } break;
    case IP_EVENT_STA_GOT_IP: {
        WiFi.enableIpV6(); // Under IDF 5 we need to get IPv4 address first.

        gotIpAddr = true;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        Serial.print("% IPv4 Address: ");
        Serial.println(IPAddress(event->ip_info.ip.addr));
    } break;
    case IP_EVENT_STA_LOST_IP:
        // gotIpAddr = false;
    default:
        break;
    }
}


void controlTask(void *pvParameter) {
    // Mount the file system.
    /*
    boolean fsGood = SPIFFS.begin();
    if (!fsGood) {
        printf("%% No formatted SPIFFS filesystem found to mount.\n");
        printf("%% Format SPIFFS and mount now (NB. may cause data loss) [y/n]?\n");
        while (!Serial.available()) {
        }
        char c = Serial.read();
        if (c == 'y' || c == 'Y') {
            printf("%% Formatting...\n");
            fsGood = SPIFFS.format();
            if (fsGood)
                SPIFFS.begin();
        }
    }
    if (!fsGood) {
        printf("%% Aborting now.\n");
        while (1)
            vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
    printf(
        "%% Mounted SPIFFS used=%d total=%d\r\n", SPIFFS.usedBytes(),
        SPIFFS.totalBytes());

    */

    wifiPhyConnected = false;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_MODE_STA);
    gotIpAddr = false;
    gotIp6Addr = false;
    WiFi.begin(configSTASSID, configSTAPSK);

    TickType_t xStartTime;
    xStartTime = xTaskGetTickCount();
    const TickType_t xTicksTimeout = WIFI_TIMEOUT_S * 1000 / portTICK_PERIOD_MS;
    bool aborting;

    while (1) {
        switch (devState) {
        case STATE_NEW:
            vTaskDelay(NET_WAIT_MS / portTICK_PERIOD_MS);
            break;
        case STATE_PHY_CONNECTED:
            newDevState(STATE_WAIT_IPADDR);
            // Set the initial time, where timeout will be started
            xStartTime = xTaskGetTickCount();
            break;
        case STATE_WAIT_IPADDR:
            if (gotIpAddr && gotIp6Addr)
                newDevState(STATE_GOT_IPADDR);
            else {
                // Check the timeout.
                if (xTaskGetTickCount() >= xStartTime + xTicksTimeout) {
                    printf("%% Timeout waiting for all IP addresses\n");
                    if (gotIpAddr || gotIp6Addr)
                        newDevState(STATE_GOT_IPADDR);
                    else
                        newDevState(STATE_NEW);
                } else {
                    vTaskDelay(NET_WAIT_MS / portTICK_PERIOD_MS);
                }
            }
            break;
        case STATE_GOT_IPADDR:
            newDevState(STATE_OTA_UPDATING);
            break;
        case STATE_OTA_UPDATING:
            // No OTA for this sketch.
            newDevState(STATE_OTA_COMPLETE);
            break;
        case STATE_OTA_COMPLETE:
            aborting = false;
            // Initialize the Arduino library.
            libssh_begin();

            // Call the EXAMPLE main code.
            /*
            {
                const char *ex_argv[] = {EX_CMD, NULL};
                int ex_argc = sizeof ex_argv / sizeof ex_argv[0] - 1;
                printf("%% Execution in progress:");
                short a;
                for (a = 0; a < ex_argc; a++)
                    printf(" %s", ex_argv[a]);
                long start_millis = millis();
                printf("\n[SNIP STDOUT START]\n");
                int ex_rc = ex_main(ex_argc, (char **)ex_argv);
                printf("[SNIP STDOUT FINISH]\n");
                printf("%% Execution completed: rc=%d, elapsed=%ldms\n",
                       ex_rc, (long)millis() - start_millis);
            }
            */
            // Why does this not work???
            // Call the EXAMPLE main code.
            
            {



                long start_millis = millis(); // Current time
                printf("\n[SNIP STDOUT START]\n");
                int ex_rc = ex_main();
                printf("[SNIP STDOUT FINISH]\n");
                printf("%% Execution completed: rc=%d, elapsed=%ldms\n",
                       ex_rc, (long)millis() - start_millis);
            }
            
            while (1)
                vTaskDelay(60000 / portTICK_PERIOD_MS);
            // Finished the EXAMPLE main code.
            if (!aborting)
                newDevState(STATE_LISTENING);
            else
                newDevState(STATE_TCP_DISCONNECTED);
            break;
        case STATE_LISTENING:
            aborting = false;
            newDevState(STATE_TCP_DISCONNECTED);
            break;
        case STATE_TCP_DISCONNECTED:
            // This would be the place to free net resources, if needed,
            newDevState(STATE_LISTENING);
            break;
        default:
            break;
        }
    }
}


int ex_main() {
    ssh_session session = NULL;
    ssh_channel channel = NULL;
    char buffer[256];
    int rbytes, wbytes, total = 0;
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


    rc = ssh_channel_request_exec(channel, "ip a");
    if (rc < 0) { // Got to document this code. I rekon this is -1 for error code
        goto failed;
    }

    // First itter of loop. Reads all bytes into buffer
    rbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
    if (rbytes <= 0) { // This one is the NUMBER of bytes, so if 0 then no data
        goto failed;
    }
      // Try my OWN print loop

    do {
      shh_output_string += String(buffer, rbytes);
      rbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
    } while (rbytes > 0);

    Serial.println(shh_output_string);


    
    /*
    do {
        // Trys to write as many bytes as were given into buffer
       // Serial.println("Loopy doop");
        wbytes = fwrite(buffer + total, 1, rbytes, stdout);
        // fwrite first arg is a POINTER to the first byte. Then the total bytes read is counted by fwrite, and returned as wbytes.
        // This is then added to total, to point to the next first item in case of annother itteration
        // This would be neccecry if the stuff to go in the buffer was longer than 256 bytes.
        
        
        shh_output_string += String(buffer + total, rbytes);
        if (wbytes <= 0) {
            goto failed;
        }


        total += wbytes;

        // When it was not possible to write the whole buffer to stdout
        if (wbytes < rbytes) {
            rbytes -= wbytes;
            // Goes to the next itter WITHOUT reading the next bytes into the buffer because there are some left
            // Then becuase it is a while loop it will try write the bytes again.

            // This may happen because buffer is 256 chars
            continue;
        }

        rbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
        total = 0;
    } while (rbytes > 0);
    */

  

    if (rbytes < 0) {
        goto failed;
    }

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    ssh_disconnect(session);
    ssh_free(session);
    ssh_finalize();

    return 0;
failed:
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    ssh_disconnect(session);
    ssh_free(session);
    ssh_finalize();

    return 1;
}

void setup() {
    devState = STATE_NEW;

    // Use the expected blocking I/O behavior.
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    Serial.begin(115200);
    uart_driver_install((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

    esp_netif_init();
    esp_event_loop_create_default();
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_cb, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, event_cb, NULL, NULL);

    // Stack size needs to be larger, so continue in a new task.
    xTaskCreatePinnedToCore(controlTask, "ctl", configSTACK, NULL,
                            (tskIDLE_PRIORITY + 3), NULL, portNUM_PROCESSORS - 1);
}

void loop() {
    // Nothing to do here since controlTask has taken over.

    //What happens if i fold in a web server?
    //vTaskDelay(60000 / portTICK_PERIOD_MS);

  }