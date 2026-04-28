#pragma once

#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char ap_query_service[64];   /* e.g. _anker_power._udp */
    char sta_query_service[64];  /* e.g. _anker_power._udp */

    char ap_service_name[96];
    char ap_host_name[96];
    char ap_pn_key[24];
    char ap_pn[32];
    char ap_sn_key[24];
    char ap_sn[64];
    char ap_account_id_key[32];
    char ap_account_id[96];

    char sta_service_name[96];
    char sta_host_name[96];
    char sta_pn_key[24];
    char sta_pn[32];
    char sta_sn_key[24];
    char sta_sn[64];
    char sta_account_id_key[32];
    char sta_account_id[96];
} mdns_proxy_config_t;


/* Return current in-memory config */
const mdns_proxy_config_t *mdns_proxy_get_config(void);

/* Validate + save config to NVS + rebuild runtime profiles */
esp_err_t mdns_proxy_set_config(const mdns_proxy_config_t *cfg);

/* Return current ESP interface IPs as strings */
void mdns_proxy_get_interface_ips(char *ap_buf, size_t ap_len,
                                  char *sta_buf, size_t sta_len);

/* Start proxy tasks */
void start_mdns_proxy(void);

#ifdef __cplusplus
}
#endif