/*

This Whole file is not my code, and while I have made minor modifications,
It should be treated as an external library for marking purposes

*/

#include "IPv6Address.h"
#include <ssh_functions.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <libssh/libssh.h>
#include "libssh_esp32.h"

String configSTASSID;
String configSTAPSK;

extern volatile bool wifiPhyConnected;

extern WiFiMulti wifiMulti;

#define WIFI_TIMEOUT_S 10
#define NET_WAIT_MS 100

extern volatile devState_t devState;
extern volatile bool gotIpAddr, gotIp6Addr;

void wait_for_wifi_exec(std::function<int ()> exec_func) {
    wifiPhyConnected = false;
    
    gotIpAddr = false;
    gotIp6Addr = false;


    TickType_t xStartTime;
    xStartTime = xTaskGetTickCount();
    const TickType_t xTicksTimeout = WIFI_TIMEOUT_S * 1000 / portTICK_PERIOD_MS;
    bool aborting;

    while (1) {
        // This whole loop is to wait for a wifi connection
        switch (devState) {
        case STATE_NEW:
            vTaskDelay(NET_WAIT_MS / portTICK_PERIOD_MS);
            break;
        case STATE_PHY_CONNECTED:
            devState = STATE_WAIT_IPADDR;
            // Set the initial time, where timeout will be started
            xStartTime = xTaskGetTickCount();
            break;
        case STATE_WAIT_IPADDR:
            if (gotIpAddr && gotIp6Addr)
                devState = STATE_GOT_IPADDR;
            else {
                // Check the timeout.
                if (xTaskGetTickCount() >= xStartTime + xTicksTimeout) {
                    printf("%% Timeout waiting for all IP addresses\n");
                    if (gotIpAddr || gotIp6Addr)
                        devState = STATE_GOT_IPADDR;
                    else
                        devState = STATE_NEW;
                } else {
                    vTaskDelay(NET_WAIT_MS / portTICK_PERIOD_MS);
                }
            }
            break;
        case STATE_GOT_IPADDR:
            devState = STATE_OTA_UPDATING ;
            break;
        case STATE_OTA_UPDATING:
            // No OTA for this sketch.
            devState = STATE_OTA_COMPLETE ;
            break;
        case STATE_OTA_COMPLETE:
            aborting = false;
            // Initialize the Arduino library.
            libssh_begin();
            {
                long start_millis = millis(); // Current time
                printf("\n[SNIP STDOUT START]\n");
                int ex_rc = exec_func();
                printf("[SNIP STDOUT FINISH]\n");
                printf("%% Execution completed: rc=%d, elapsed=%ldms\n",
                       ex_rc, (long)millis() - start_millis);
            }
            while (1)
                vTaskDelay(60000 / portTICK_PERIOD_MS);
            // Finished the EXAMPLE main code.
            if (!aborting)
                devState = STATE_LISTENING;
            else
                devState = STATE_TCP_DISCONNECTED;
            break;
        case STATE_LISTENING:
            aborting = false;
            devState = STATE_TCP_DISCONNECTED;
            break;
        case STATE_TCP_DISCONNECTED:
            // This would be the place to free net resources, if needed,
            devState = STATE_LISTENING;
            break;
        default:
            break;
        }
    }
}

void event_cb(void *args, esp_event_base_t base, int32_t id, void *event_data) {
    switch (id) {
    case WIFI_EVENT_STA_START:
        Serial.println("Connecting to wifi...");
        
        break;
    case WIFI_EVENT_STA_CONNECTED:
        Serial.println("% WiFi connected");
        //configSTASSID = WiFi.SSID();
        //configSTAPSK = WiFi.psk();
        //Serial.println(configSTASSID);
        wifiPhyConnected = true;
        if (devState < STATE_PHY_CONNECTED)
            devState = STATE_PHY_CONNECTED;
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        if (devState < STATE_WAIT_IPADDR)
            devState = STATE_NEW;
        if (wifiPhyConnected) {
            Serial.println("% WiFi disconnected");
            wifiPhyConnected = false;
        }
        wifiMulti.run();
        Serial.println("Wifi disconnect. Reconnecting with wifiMulti");
        //WiFi.begin(configSTASSID, configSTAPSK);
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

int authenticate_kbdint(ssh_session session, const char *password) {
    int err;

    err = ssh_userauth_kbdint(session, NULL, NULL);
    while (err == SSH_AUTH_INFO) {
        const char *instruction;
        const char *name;
        char buffer[128];
        int i, n;

        name = ssh_userauth_kbdint_getname(session);
        instruction = ssh_userauth_kbdint_getinstruction(session);
        n = ssh_userauth_kbdint_getnprompts(session);

        if (name && strlen(name) > 0) {
            printf("%s\n", name);
        }

        if (instruction && strlen(instruction) > 0) {
            printf("%s\n", instruction);
        }

        for (i = 0; i < n; i++) {
            const char *answer;
            const char *prompt;
            char echo;

            prompt = ssh_userauth_kbdint_getprompt(session, i, &echo);
            if (prompt == NULL) {
                break;
            }

            if (echo) {
                char *p;

                printf("%s", prompt);

                if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                    return SSH_AUTH_ERROR;
                }

                buffer[sizeof(buffer) - 1] = '\0';
                if ((p = strchr(buffer, '\n'))) {
                    *p = '\0';
                }

                if (ssh_userauth_kbdint_setanswer(session, i, buffer) < 0) {
                    return SSH_AUTH_ERROR;
                }

                memset(buffer, 0, strlen(buffer));
            } else {
                if (password && strstr(prompt, "Password:")) {
                    answer = password;
                } else {
                    buffer[0] = '\0';

                    if (ssh_getpass(prompt, buffer, sizeof(buffer), 0, 0) < 0) {
                        return SSH_AUTH_ERROR;
                    }
                    answer = buffer;
                }
                err = ssh_userauth_kbdint_setanswer(session, i, answer);
                memset(buffer, 0, sizeof(buffer));
                if (err < 0) {
                    return SSH_AUTH_ERROR;
                }
            }
        }
        err = ssh_userauth_kbdint(session, NULL, NULL);
    }

    return err;
}

static int auth_keyfile(ssh_session session, char *keyfile) {
    ssh_key key = NULL;
    char pubkey[132] = {0}; // +".pub"
    int rc;

    snprintf(pubkey, sizeof(pubkey), "%s.pub", keyfile);

    rc = ssh_pki_import_pubkey_file(pubkey, &key);

    if (rc != SSH_OK)
        return SSH_AUTH_DENIED;

    rc = ssh_userauth_try_publickey(session, NULL, key);

    ssh_key_free(key);

    if (rc != SSH_AUTH_SUCCESS)
        return SSH_AUTH_DENIED;

    rc = ssh_pki_import_privkey_file(keyfile, NULL, NULL, NULL, &key);

    if (rc != SSH_OK)
        return SSH_AUTH_DENIED;

    rc = ssh_userauth_publickey(session, NULL, key);
    ssh_key_free(key);

    return rc;
}

static void error(ssh_session session) {
    fprintf(stderr, "Authentication failed: %s\n", ssh_get_error(session));
}

int authenticate_console(ssh_session session, char *password) {
    int rc;
    int method;
    // char password[128] = {0};
    char *banner;

    // Try to authenticate
    rc = ssh_userauth_none(session, NULL);
    if (rc == SSH_AUTH_ERROR) {
        error(session);
        return rc;
    }

    method = ssh_userauth_list(session, NULL);
    while (rc != SSH_AUTH_SUCCESS) {
        if (method & SSH_AUTH_METHOD_GSSAPI_MIC) {
            rc = ssh_userauth_gssapi(session);
            if (rc == SSH_AUTH_ERROR) {
                error(session);
                return rc;
            } else if (rc == SSH_AUTH_SUCCESS) {
                break;
            }
        }
        // Try to authenticate with public key first
        //  if (method & SSH_AUTH_METHOD_PUBLICKEY) {
        //      rc = ssh_userauth_publickey_auto(session, NULL, NULL);
        //      if (rc == SSH_AUTH_ERROR) {
        //          error(session);
        //          return rc;
        //      } else if (rc == SSH_AUTH_SUCCESS) {
        //          break;
        //      }
        //  }

        /*
          {
              char buffer[128] = {0};
              char *p = NULL;

              printf("Automatic pubkey failed. "
                     "Do you want to try a specific key? (y/n)\n");
              if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                  break;
              }
              if ((buffer[0] == 'Y') || (buffer[0] == 'y')) {
                  printf("private key filename: ");

                  if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                      return SSH_AUTH_ERROR;
                  }

                  buffer[sizeof(buffer) - 1] = '\0';
                  if ((p = strchr(buffer, '\n'))) {
                      *p = '\0';
                  }

                  rc = auth_keyfile(session, buffer);

                  if (rc == SSH_AUTH_SUCCESS) {
                      break;
                  }
                  fprintf(stderr, "failed with key\n");
              }
          }
          */

        // This code is for getting user input over Serial for password. I am using set password, so not needed.

        /*

        // Try to authenticate with keyboard interactive";
        if (method & SSH_AUTH_METHOD_INTERACTIVE) {
            rc = authenticate_kbdint(session, NULL);
            if (rc == SSH_AUTH_ERROR) {
                error(session);
                return rc;
            } else if (rc == SSH_AUTH_SUCCESS) {
                break;
            }
        }

        if (ssh_getpass("Password: ", password, sizeof(password), 0, 0) < 0) {
            return SSH_AUTH_ERROR;
        }

        */

        // Try to authenticate with password
        if (method & SSH_AUTH_METHOD_PASSWORD) {
            rc = ssh_userauth_password(session, NULL, password);
            if (rc == SSH_AUTH_ERROR) {
                error(session);
                return rc;
            } else if (rc == SSH_AUTH_SUCCESS) {
                break;
            }
        }
        memset(password, 0, sizeof(password));
    }

    banner = ssh_get_issue_banner(session);
    if (banner) {
        printf("%s\n", banner);
        SSH_STRING_FREE_CHAR(banner);
    }

    return rc;
}

ssh_session connect_ssh(char *password, const char *host, const char *user, int verbosity) {
    ssh_session session;
    int auth = 0;

    session = ssh_new();
    if (session == NULL) {
        return NULL;
    }

    if (user != NULL) {
        if (ssh_options_set(session, SSH_OPTIONS_USER, user) < 0) {
            ssh_free(session);
            return NULL;
        }
    }

    if (ssh_options_set(session, SSH_OPTIONS_HOST, host) < 0) {
        ssh_free(session);
        return NULL;
    }
    ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
    if (ssh_connect(session)) {
        fprintf(stderr, "Connection failed : %s\n", ssh_get_error(session));
        ssh_disconnect(session);
        ssh_free(session);
        return NULL;
    }
    auth = authenticate_console(session, password);
    if (auth == SSH_AUTH_SUCCESS) {
        return session;
    } else if (auth == SSH_AUTH_DENIED) {
        fprintf(stderr, "Authentication failed\n");
    } else {
        fprintf(stderr, "Error while authenticating : %s\n", ssh_get_error(session));
    }
    ssh_disconnect(session);
    ssh_free(session);
    return NULL;
}
