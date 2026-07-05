#ifndef SSH_FUNCTIONS_H
#define SSH_FUNCTIONS_H

#include <ssh_functions.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <libssh/libssh.h>


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

extern const unsigned int configSTACK;
extern volatile bool wifiPhyConnected;
void wait_for_wifi_exec(std::function<int ()> exec_func);
int authenticate_kbdint(ssh_session session,const char *password);
int authenticate_console(ssh_session session,char *password);
ssh_session connect_ssh(char *password,const char *host,const char *user,int verbosity);
void event_cb(void *args, esp_event_base_t base, int32_t id, void *event_data);
#define INTERFACE 0
#define EXPORT_INTERFACE 0
#define LOCAL_INTERFACE 0
#define EXPORT
#define LOCAL static
#define PUBLIC
#define PRIVATE
#define PROTECTED
#endif // SSH_FUNCTIONS_H
