#pragma once

#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char ap_target_ip[16];
    char sta_target_ip[16];

    char ap_service_name[96];
    char ap_mac[18];
    char ap_pn[32];
    char ap_sn[64];
    char ap_account_id[96];

    char sta_service_name[96];
    char sta_mac[18];
    char sta_pn[32];
    char sta_sn[64];
    char sta_account_id[96];
} mdns_proxy_config_t;

const mdns_proxy_config_t *mdns_proxy_get_config(void);
esp_err_t mdns_proxy_set_config(const mdns_proxy_config_t *cfg);
void mdns_proxy_get_interface_ips(char *ap_buf, size_t ap_len,
                                  char *sta_buf, size_t sta_len);
void start_mdns_proxy(void);

#ifdef __cplusplus
}
#endif
