#ifndef SSH_FUNCTIONS_H
#define SSH_FUNCTIONS_H

#include "SPIFFS.h"
#include "libssh/priv.h"
#include "libssh_esp32.h"
#include "libssh_esp32_config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <arpa/inet.h>
#include <libssh/libssh.h>
#include <stdlib.h>
#include <ssh_functions.h>

extern const unsigned int configSTACK;
extern volatile bool wifiPhyConnected;
int authenticate_kbdint(ssh_session session,const char *password);
int authenticate_console(ssh_session session,char *password);
ssh_session connect_ssh(char *password,const char *host,const char *user,int verbosity);
#define INTERFACE 0
#define EXPORT_INTERFACE 0
#define LOCAL_INTERFACE 0
#define EXPORT
#define LOCAL static
#define PUBLIC
#define PRIVATE
#define PROTECTED
#endif // SSH_FUNCTIONS_H
