// ============================================================
// multicast_proxy.c
// Version: 0.5.0
//
// FIXES:
// - mDNS response now much closer to live environment
// - PTR + SRV + TXT + A(additional)
// - TTLs aligned to sniff
// - TXT fields for Smartmeter / Solarbank included
// - spoofed A record points to local ESP IP on each side
// ============================================================

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "esp_log.h"
#include "esp_err.h"
#include "nvs.h"
#include "lwip/ip4_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "router_config.h"
#include "router_globals.h"
#include "multicast_proxy.h"

static const char *TAG = "MDNS_PROXY_V0_5_0";

// ------------------------------------------------------------
// Network config
// ------------------------------------------------------------

#define MDNS_PROXY_CFG_KEY "mdns_proxy_cfg"

// ------------------------------------------------------------
// mDNS profile
// ------------------------------------------------------------

typedef enum {
    MDNS_SIDE_AP = 0,
    MDNS_SIDE_STA = 1
} mdns_side_t;

typedef struct {
    mdns_side_t side;
    char iface_ip[16];
    char query_service[64];

    char service_name[96];
    char host_name[96];
    char spoof_ip[16];

    char pn_key[24];
    char pn[32];
    char sn_key[24];
    char sn[64];
    char account_id_key[32];
    char account_id[96];
} mdns_profile_t;

static mdns_proxy_config_t g_mdns_cfg = {
    .ap_query_service = "_anker_power._udp",
    .sta_query_service = "_anker_power._udp",

            .ap_service_name      = "anker wifi thing._anker_power._udp.local",
            .ap_host_name         = "Anker-Device_f4:9d:8a:aa:bb:cc.local",
            .ap_pn_key            = "pn",
            .ap_pn                = "A17X7",
            .ap_sn_key            = "sn",
            .ap_sn                = "AUABC123",
            .ap_account_id_key    = "account_id",
            .ap_account_id        = "12345678",

            .sta_service_name     = "anker wifi thing-2._anker_power._udp.local",
            .sta_host_name        = "Anker-Device_f4:9d:8a:cc:dd:ee.local",
            .sta_pn_key           = "pn",
            .sta_pn               = "A17C1",
            .sta_sn_key           = "sn",
            .sta_sn               = "APDEF456",
            .sta_account_id_key   = "account_id",
            .sta_account_id       = "12345678"
        };		

static mdns_profile_t g_ap_profile;
static mdns_profile_t g_sta_profile;

static void ip4_to_str(uint32_t ip, char *buf, size_t len)
{
    ip4_addr_t a;
    a.addr = ip;
    ip4addr_ntoa_r(&a, buf, len);
}

static bool ip_in_same_24(uint32_t a, uint32_t b)
{
    return (a & 0xFFFFFF00UL) == (b & 0xFFFFFF00UL);
}

static bool is_valid_txt_key(const char *s)
{
    if (!s || s[0] == '\0') {
        return false;
    }

    if (strlen(s) > 31) {
        return false;
    }

    if (strchr(s, '=') != NULL) {
        return false;
    }

    return true;
}

static bool is_valid_query_service(const char *s)
{
    if (!s || s[0] == '\0') {
        return false;
    }

    if (strlen(s) >= 64) {
        return false;
    }

    if (strchr(s, ' ') != NULL) {
        return false;
    }

    if (strchr(s, '.') == NULL) {
        return false;
    }

    return true;
}

static esp_err_t mdns_proxy_cfg_load_internal(void)
{
    nvs_handle_t nvs;
    size_t len = sizeof(g_mdns_cfg);

    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_get_blob(nvs, MDNS_PROXY_CFG_KEY, &g_mdns_cfg, &len);

    bool reset_to_defaults = false;

    if (err == ESP_OK && len == sizeof(g_mdns_cfg)) {
        if (!is_valid_query_service(g_mdns_cfg.ap_query_service) ||
            !is_valid_query_service(g_mdns_cfg.sta_query_service) ||
            g_mdns_cfg.ap_host_name[0] == '\0' ||
            g_mdns_cfg.sta_host_name[0] == '\0' ||
            !is_valid_txt_key(g_mdns_cfg.ap_pn_key) ||
            !is_valid_txt_key(g_mdns_cfg.ap_sn_key) ||
            !is_valid_txt_key(g_mdns_cfg.ap_account_id_key) ||
            !is_valid_txt_key(g_mdns_cfg.sta_pn_key) ||
            !is_valid_txt_key(g_mdns_cfg.sta_sn_key) ||
            !is_valid_txt_key(g_mdns_cfg.sta_account_id_key)) {

            ESP_LOGW(TAG, "mdns_proxy config invalid, resetting to defaults");
            reset_to_defaults = true;
        }

    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "mdns_proxy config not found, writing defaults");
        reset_to_defaults = true;

    } else if (err == ESP_OK && len != sizeof(g_mdns_cfg)) {
        ESP_LOGW(TAG, "mdns_proxy config size changed (%u -> %u), resetting to defaults",
                 (unsigned)len, (unsigned)sizeof(g_mdns_cfg));
        reset_to_defaults = true;

    } else {
        ESP_LOGW(TAG, "mdns_proxy config load failed (%s), resetting to defaults",
                 esp_err_to_name(err));
        reset_to_defaults = true;
    }

    if (reset_to_defaults) {
        mdns_proxy_config_t defaults = {
            .ap_query_service = "_anker_power._udp",
            .sta_query_service = "_anker_power._udp",

            .ap_service_name      = "anker wifi thing._anker_power._udp.local",
            .ap_host_name         = "Anker-Device_f4:9d:8a:aa:bb:cc.local",
            .ap_pn_key            = "pn",
            .ap_pn                = "A17X7",
            .ap_sn_key            = "sn",
            .ap_sn                = "AUABC123",
            .ap_account_id_key    = "account_id",
            .ap_account_id        = "12345678",

            .sta_service_name     = "anker wifi thing-2._anker_power._udp.local",
            .sta_host_name        = "Anker-Device_f4:9d:8a:cc:dd:ee.local",
            .sta_pn_key           = "pn",
            .sta_pn               = "A17C1",
            .sta_sn_key           = "sn",
            .sta_sn               = "APDEF456",
            .sta_account_id_key   = "account_id",
            .sta_account_id       = "12345678"
        };		
        
        memcpy(&g_mdns_cfg, &defaults, sizeof(g_mdns_cfg));

        err = nvs_set_blob(nvs, MDNS_PROXY_CFG_KEY, &g_mdns_cfg, sizeof(g_mdns_cfg));
        if (err == ESP_OK) {
            err = nvs_commit(nvs);
        }
    }

    nvs_close(nvs);
    return err;
}

static esp_err_t mdns_proxy_cfg_save_internal(const mdns_proxy_config_t *cfg)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(nvs, MDNS_PROXY_CFG_KEY, cfg, sizeof(*cfg));
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

static void copy_trunc(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void build_profiles_from_runtime(void)
{
    memset(&g_ap_profile, 0, sizeof(g_ap_profile));
    memset(&g_sta_profile, 0, sizeof(g_sta_profile));

    /* AP side */
    g_ap_profile.side = MDNS_SIDE_AP;
    ip4_to_str(my_ap_ip, g_ap_profile.iface_ip, sizeof(g_ap_profile.iface_ip));
    ip4_to_str(my_ap_ip, g_ap_profile.spoof_ip, sizeof(g_ap_profile.spoof_ip));

    copy_trunc(g_ap_profile.query_service, sizeof(g_ap_profile.query_service), g_mdns_cfg.ap_query_service);
    copy_trunc(g_ap_profile.service_name, sizeof(g_ap_profile.service_name), g_mdns_cfg.ap_service_name);

    copy_trunc(g_ap_profile.host_name,
           sizeof(g_ap_profile.host_name),
           g_mdns_cfg.ap_host_name);

    copy_trunc(g_ap_profile.pn_key, sizeof(g_ap_profile.pn_key), g_mdns_cfg.ap_pn_key);
    copy_trunc(g_ap_profile.pn, sizeof(g_ap_profile.pn), g_mdns_cfg.ap_pn);

    copy_trunc(g_ap_profile.sn_key, sizeof(g_ap_profile.sn_key), g_mdns_cfg.ap_sn_key);
    copy_trunc(g_ap_profile.sn, sizeof(g_ap_profile.sn), g_mdns_cfg.ap_sn);

    copy_trunc(g_ap_profile.account_id_key, sizeof(g_ap_profile.account_id_key), g_mdns_cfg.ap_account_id_key);
    copy_trunc(g_ap_profile.account_id, sizeof(g_ap_profile.account_id), g_mdns_cfg.ap_account_id);

    /* STA side */
    g_sta_profile.side = MDNS_SIDE_STA;
    ip4_to_str(my_ip, g_sta_profile.iface_ip, sizeof(g_sta_profile.iface_ip));
    ip4_to_str(my_ip, g_sta_profile.spoof_ip, sizeof(g_sta_profile.spoof_ip));

    copy_trunc(g_sta_profile.query_service, sizeof(g_sta_profile.query_service), g_mdns_cfg.sta_query_service);
    copy_trunc(g_sta_profile.service_name, sizeof(g_sta_profile.service_name), g_mdns_cfg.sta_service_name);

    copy_trunc(g_sta_profile.host_name,
           sizeof(g_sta_profile.host_name),
           g_mdns_cfg.sta_host_name);

    copy_trunc(g_sta_profile.pn_key, sizeof(g_sta_profile.pn_key), g_mdns_cfg.sta_pn_key);
    copy_trunc(g_sta_profile.pn, sizeof(g_sta_profile.pn), g_mdns_cfg.sta_pn);

    copy_trunc(g_sta_profile.sn_key, sizeof(g_sta_profile.sn_key), g_mdns_cfg.sta_sn_key);
    copy_trunc(g_sta_profile.sn, sizeof(g_sta_profile.sn), g_mdns_cfg.sta_sn);

    copy_trunc(g_sta_profile.account_id_key, sizeof(g_sta_profile.account_id_key), g_mdns_cfg.sta_account_id_key);
    copy_trunc(g_sta_profile.account_id, sizeof(g_sta_profile.account_id), g_mdns_cfg.sta_account_id);
}

const mdns_proxy_config_t *mdns_proxy_get_config(void)
{
    return &g_mdns_cfg;
}

void mdns_proxy_get_interface_ips(char *ap_buf, size_t ap_len,
                                  char *sta_buf, size_t sta_len)
{
    if (ap_buf && ap_len > 0) ip4_to_str(my_ap_ip, ap_buf, ap_len);
    if (sta_buf && sta_len > 0) ip4_to_str(my_ip, sta_buf, sta_len);
}



esp_err_t mdns_proxy_set_config(const mdns_proxy_config_t *cfg)
{
    mdns_proxy_config_t tmp_cfg;

    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!is_valid_query_service(g_mdns_cfg.ap_query_service) || !is_valid_query_service(g_mdns_cfg.sta_query_service)) {
    return ESP_ERR_INVALID_ARG;
    }

    if (!is_valid_query_service(cfg->sta_query_service)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!is_valid_txt_key(cfg->ap_pn_key)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!is_valid_txt_key(cfg->ap_sn_key)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!is_valid_txt_key(cfg->ap_account_id_key)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!is_valid_txt_key(cfg->sta_pn_key)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!is_valid_txt_key(cfg->sta_sn_key)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!is_valid_txt_key(cfg->sta_account_id_key)) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&tmp_cfg, cfg, sizeof(tmp_cfg));

if (cfg->ap_host_name[0] == '\0') {
    return ESP_ERR_INVALID_ARG;
}

if (cfg->sta_host_name[0] == '\0') {
    return ESP_ERR_INVALID_ARG;
}

    memcpy(&g_mdns_cfg, &tmp_cfg, sizeof(g_mdns_cfg));

    esp_err_t err = mdns_proxy_cfg_save_internal(&g_mdns_cfg);
    if (err == ESP_OK) {
        build_profiles_from_runtime();
    }

    return err;
}

static size_t encode_dns_name_no_root(const char *name, uint8_t *out, size_t out_len)
{
    const char *p = name;
    size_t used = 0;

    while (*p) {
        const char *dot = strchr(p, '.');
        size_t label_len = dot ? (size_t)(dot - p) : strlen(p);

        if (label_len == 0) {
            p++;
            continue;
        }

        if (label_len > 63 || used + 1 + label_len > out_len) {
            return 0;
        }

        out[used++] = (uint8_t)label_len;
        memcpy(out + used, p, label_len);
        used += label_len;

        if (!dot) {
            break;
        }
        p = dot + 1;
    }

    return used;
}

// ------------------------------------------------------------
// DNS encoder helpers
// ------------------------------------------------------------

static uint8_t* write_name(uint8_t *p, const char *name)
{
    while (*name) {
        const char *dot = strchr(name, '.');
        int len = dot ? (int)(dot - name) : (int)strlen(name);

        *p++ = (uint8_t)len;
        memcpy(p, name, len);
        p += len;

        if (!dot) break;
        name = dot + 1;
    }
    *p++ = 0;
    return p;
}

static uint8_t *write_u16(uint8_t *p, uint16_t v)
{
    *p++ = (v >> 8) & 0xff;
    *p++ = v & 0xff;
    return p;
}

static uint8_t *write_u32(uint8_t *p, uint32_t v)
{
    *p++ = (v >> 24) & 0xff;
    *p++ = (v >> 16) & 0xff;
    *p++ = (v >> 8) & 0xff;
    *p++ = v & 0xff;
    return p;
}

static uint8_t *write_txt_kv(uint8_t *p, const char *kv)
{
    size_t len = strlen(kv);
    *p++ = (uint8_t)len;
    memcpy(p, kv, len);
    return p + len;
}

// ------------------------------------------------------------
// mDNS response builder (live-like)
// ------------------------------------------------------------

static int build_mdns_response_live(uint8_t *resp, const mdns_profile_t *profile)
{
    uint8_t *p = resp;
    uint8_t *rdlength_ptr;
    uint8_t *start;
    uint16_t rdlength;

    uint8_t ip[4];
    sscanf(profile->spoof_ip, "%hhu.%hhu.%hhu.%hhu",
           &ip[0], &ip[1], &ip[2], &ip[3]);

    /* mDNS header
     * ID=0, Flags=0x8400, QD=0, AN=3, NS=0, AR=1
     */
    p = write_u16(p, 0x0000);
    p = write_u16(p, 0x8400);
    p = write_u16(p, 0x0000);
    p = write_u16(p, 0x0003);
    p = write_u16(p, 0x0000);
    p = write_u16(p, 0x0001);

    /* -------------------------------------------------
     * Answer 1: PTR _anker_power._udp.local -> service_name
     * TTL 4500, class IN, no cache flush
     * ------------------------------------------------- */
    p = write_name(p, "_anker_power._udp.local");
    p = write_u16(p, 12);       // PTR
    p = write_u16(p, 0x0001);   // IN
    p = write_u32(p, 4500);

    rdlength_ptr = p; p += 2;
    start = p;
    p = write_name(p, profile->service_name);
    rdlength = (uint16_t)(p - start);
    rdlength_ptr[0] = (rdlength >> 8) & 0xff;
    rdlength_ptr[1] = rdlength & 0xff;

    /* -------------------------------------------------
     * Answer 2: SRV service_name -> host_name:8899
     * TTL 120, class IN | cache flush
     * ------------------------------------------------- */
    p = write_name(p, profile->service_name);
    p = write_u16(p, 33);       // SRV
    p = write_u16(p, 0x8001);   // IN + cache flush
    p = write_u32(p, 120);

    rdlength_ptr = p; p += 2;
    start = p;
    p = write_u16(p, 0);        // priority
    p = write_u16(p, 0);        // weight
    p = write_u16(p, 8899);     // port
    p = write_name(p, profile->host_name);
    rdlength = (uint16_t)(p - start);
    rdlength_ptr[0] = (rdlength >> 8) & 0xff;
    rdlength_ptr[1] = rdlength & 0xff;

    /* -------------------------------------------------
     * Answer 3: TXT service_name
     * TTL 4500, class IN | cache flush
     * ------------------------------------------------- */
    p = write_name(p, profile->service_name);
    p = write_u16(p, 16);       // TXT
    p = write_u16(p, 0x8001);   // IN + cache flush
    p = write_u32(p, 4500);

    rdlength_ptr = p; p += 2;
    start = p;

    p = write_txt_kv(p, "dev_type=center");

    {
        char account_buf[sizeof(profile->account_id) + sizeof(profile->account_id_key) + 2];
        snprintf(account_buf, sizeof(account_buf), "%s=%s", profile->account_id_key, profile->account_id);
        p = write_txt_kv(p, account_buf);
    }

    p = write_txt_kv(p, "app_id=anker_power");

    {
        char pn_buf[sizeof(profile->pn) + sizeof(profile->pn_key) + 2];
        snprintf(pn_buf, sizeof(pn_buf), "%s=%s", profile->pn_key, profile->pn);
        p = write_txt_kv(p, pn_buf);
    }

    {
        char sn_buf[sizeof(profile->sn) + sizeof(profile->sn_key) + 2];
        snprintf(sn_buf, sizeof(sn_buf), "%s=%s", profile->sn_key, profile->sn);
        p = write_txt_kv(p, sn_buf);
    }

    rdlength = (uint16_t)(p - start);
    rdlength_ptr[0] = (rdlength >> 8) & 0xff;
    rdlength_ptr[1] = rdlength & 0xff;

    /* -------------------------------------------------
     * Additional 1: A host_name -> spoof_ip
     * TTL 120, class IN | cache flush
     * ------------------------------------------------- */
    p = write_name(p, profile->host_name);
    p = write_u16(p, 1);        // A
    p = write_u16(p, 0x8001);   // IN + cache flush
    p = write_u32(p, 120);
    p = write_u16(p, 4);
    *p++ = ip[0];
    *p++ = ip[1];
    *p++ = ip[2];
    *p++ = ip[3];

    return (int)(p - resp);
}

// ------------------------------------------------------------
// mDNS socket
// ------------------------------------------------------------

static int create_mdns_socket(const char *iface_ip)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "[%s] socket() failed", iface_ip);
        return -1;
    }

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(5353),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "[%s] bind(5353) failed", iface_ip);
        close(sock);
        return -1;
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
    mreq.imr_interface.s_addr = inet_addr(iface_ip);

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        ESP_LOGE(TAG, "[%s] IP_ADD_MEMBERSHIP failed", iface_ip);
        close(sock);
        return -1;
    }

    return sock;
}

// ------------------------------------------------------------
// mDNS task
// ------------------------------------------------------------

static void mdns_task(void *arg)
{
    const mdns_profile_t *profile = (const mdns_profile_t *)arg;
    const char *iface_ip = profile->iface_ip;

    int sock = create_mdns_socket(iface_ip);
    if (sock < 0) {
        ESP_LOGE(TAG, "[%s] mDNS socket creation failed", iface_ip);
        vTaskDelete(NULL);
        return;
    }

    uint8_t buf[512];
    uint8_t resp[512];

    uint8_t pattern[96];
    size_t pattern_len = encode_dns_name_no_root(profile->query_service,
                                             pattern,
                                             sizeof(pattern));

    if (pattern_len == 0) {
        ESP_LOGE(TAG, "[%s] Invalid mDNS query service: %s",
                 iface_ip, profile->query_service);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    bool is_ap = (profile->side == MDNS_SIDE_AP);

    while (1) {
        struct sockaddr_in src;
        socklen_t slen = sizeof(src);
        
        int len = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr*)&src, &slen);

        if (len <= 0) continue;

        uint32_t ip = ntohl(src.sin_addr.s_addr);

        /* only accept requests from local subnet of that side */
        {
            uint32_t local_ip = ntohl(is_ap ? my_ap_ip : my_ip);
            if (!ip_in_same_24(ip, local_ip)) continue;
        }

        /* only react to _anker_power._udp */
        if (!memmem(buf, len, pattern, pattern_len)) continue;

        ESP_LOGI(TAG, "[%s] mDNS query from %s",
                 iface_ip, inet_ntoa(src.sin_addr));

        int resp_len = build_mdns_response_live(resp, profile);

        if (sendto(sock, resp, resp_len, 0,
                   (struct sockaddr*)&src, sizeof(src)) < 0) {
            ESP_LOGE(TAG, "[%s] sendto() failed", iface_ip);
        } else {
            ESP_LOGI(TAG, "[%s] TX %d bytes", iface_ip, resp_len);
        }
    }
}


// ------------------------------------------------------------
// START
// ------------------------------------------------------------

void start_mdns_proxy(void)
{
    esp_err_t err = mdns_proxy_cfg_load_internal();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mdns_proxy config load failed: %s (using defaults)",
                 esp_err_to_name(err));
    }

    build_profiles_from_runtime();
    // init_udp();

    xTaskCreate(mdns_task, "mdns_ap", 4096, (void*)&g_ap_profile, 5, NULL);
    xTaskCreate(mdns_task, "mdns_sta", 4096, (void*)&g_sta_profile, 5, NULL);

    // xTaskCreate(udp_ap_to_sta, "udp_ap_sta", 4096, NULL, 5, NULL);
    // xTaskCreate(udp_sta_to_ap, "udp_sta_ap", 4096, NULL, 5, NULL);
}