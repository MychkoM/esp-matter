// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <esp_check.h>
#include <esp_log.h>
#include <esp_matter_console.h>
#if CONFIG_ENABLE_WIFI_AP || CONFIG_ENABLE_WIFI_STATION
#include <esp_wifi.h>
#if CONFIG_ESP_MATTER_CONSOLE_USE_ESP_CONSOLE
#include <esp_matter_core.h>
#include <esp_netif.h>
#include <platform/ConnectivityManager.h>
#include <platform/ESP32/NetworkCommissioningDriver.h>
#endif
#endif // CONFIG_ENABLE_WIFI_AP || CONFIG_ENABLE_WIFI_STATION
#include <string.h>

namespace esp_matter {
namespace console {
#if CONFIG_ENABLE_WIFI_AP || CONFIG_ENABLE_WIFI_STATION
static const char *TAG = "esp_matter_console_wifi";
static engine wifi_console;

static esp_err_t wifi_connect_handler(int argc, char *argv[])
{
    ESP_RETURN_ON_FALSE(argc == 2, ESP_ERR_INVALID_ARG, TAG, "Incorrect arguments");
#if CONFIG_ESP_MATTER_CONSOLE_USE_ESP_CONSOLE
    const size_t ssid_length = strlen(argv[0]);
    const size_t password_length = strlen(argv[1]);
    ESP_RETURN_ON_FALSE(ssid_length > 0 && ssid_length <= 32 && password_length <= 64,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid SSID or password length");
    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    CHIP_ERROR err = chip::DeviceLayer::NetworkCommissioning::ESPWiFiDriver::GetInstance().ConnectWiFiNetwork(
        argv[0], static_cast<uint8_t>(ssid_length), argv[1], static_cast<uint8_t>(password_length));
    ESP_RETURN_ON_FALSE(err == CHIP_NO_ERROR, ESP_FAIL, TAG, "Failed to configure WiFi: %s", err.AsString());
#else
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    ESP_RETURN_ON_FALSE(err == ESP_OK || err == ESP_ERR_INVALID_STATE, err, TAG, "Failed to initialize WiFi");
    ESP_RETURN_ON_ERROR(esp_wifi_stop(), TAG, "Failed to stop WiFi");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Failed to set WiFi mode");
    wifi_config_t wifi_cfg = {0};
    snprintf((char *)wifi_cfg.sta.ssid, sizeof(wifi_cfg.sta.ssid), "%s", argv[0]);
    snprintf((char *)wifi_cfg.sta.password, sizeof(wifi_cfg.sta.password), "%s", argv[1]);
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg), TAG, "Failed to set WiFi configuration");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Failed to start WiFi");
    ESP_RETURN_ON_ERROR(esp_wifi_connect(), TAG, "Failed to connect WiFi");
#endif
    return ESP_OK;
}

#if CONFIG_ESP_MATTER_CONSOLE_USE_ESP_CONSOLE
static esp_err_t wifi_status_handler(int argc, char *argv[])
{
    ESP_RETURN_ON_FALSE(argc == 0, ESP_ERR_INVALID_ARG, TAG, "Incorrect arguments");
    wifi_config_t config = {};
    wifi_country_t country = {};
    ESP_RETURN_ON_ERROR(esp_wifi_get_config(WIFI_IF_STA, &config), TAG, "Failed to read WiFi configuration");
    ESP_RETURN_ON_ERROR(esp_wifi_get_country(&country), TAG, "Failed to read WiFi country");
    ESP_LOGI(TAG, "Configured SSID: '%.*s', country: %.2s, channels: %u-%u", 32, config.sta.ssid,
             country.cc, country.schan, country.schan + country.nchan - 1);
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    ESP_RETURN_ON_FALSE(netif != nullptr, ESP_ERR_INVALID_STATE, TAG, "WiFi STA interface is unavailable");
    esp_netif_dhcp_status_t dhcp_status;
    esp_netif_ip_info_t ip_info = {};
    ESP_RETURN_ON_ERROR(esp_netif_dhcpc_get_status(netif, &dhcp_status), TAG, "Failed to read DHCP status");
    ESP_RETURN_ON_ERROR(esp_netif_get_ip_info(netif, &ip_info), TAG, "Failed to read IP address");
    ESP_LOGI(TAG, "DHCP client: %s, IPv4: " IPSTR ", gateway: " IPSTR,
             dhcp_status == ESP_NETIF_DHCP_STARTED ? "started" : "not started", IP2STR(&ip_info.ip), IP2STR(&ip_info.gw));
    return ESP_OK;
}

static esp_err_t wifi_scan_handler(int argc, char *argv[])
{
    ESP_RETURN_ON_FALSE(argc == 0, ESP_ERR_INVALID_ARG, TAG, "Incorrect arguments");
    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    auto &connectivity = chip::DeviceLayer::ConnectivityMgr();
    const auto previous_mode = connectivity.GetWiFiStationMode();
    CHIP_ERROR mode_err = connectivity.SetWiFiStationMode(
        chip::DeviceLayer::ConnectivityManager::kWiFiStationMode_ApplicationControlled);
    ESP_RETURN_ON_FALSE(mode_err == CHIP_NO_ERROR, ESP_FAIL, TAG, "Failed to pause automatic connection");
    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "Disconnect before scan: %s", esp_err_to_name(err));
    }
    wifi_scan_config_t scan_config = {};
    scan_config.show_hidden = true;
    scan_config.scan_type = WIFI_SCAN_TYPE_PASSIVE;
    scan_config.scan_time.passive = 360;
    err = esp_wifi_scan_start(&scan_config, true);
    if (err == ESP_OK) {
        uint16_t count = 0;
        err = esp_wifi_scan_get_ap_num(&count);
        ESP_LOGI(TAG, "Scan found %u access points", count);
        for (uint16_t index = 0; err == ESP_OK && index < count; ++index) {
            wifi_ap_record_t record = {};
            err = esp_wifi_scan_get_ap_record(&record);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "SSID: '%.*s', channel: %u, RSSI: %d, authmode: %u", 32, record.ssid,
                         record.primary, record.rssi, record.authmode);
            }
        }
        esp_wifi_clear_ap_list();
    }
    mode_err = connectivity.SetWiFiStationMode(previous_mode);
    ESP_RETURN_ON_ERROR(err, TAG, "WiFi scan failed");
    ESP_RETURN_ON_FALSE(mode_err == CHIP_NO_ERROR, ESP_FAIL, TAG, "Failed to restore automatic connection");
    return ESP_OK;
}
#endif

static esp_err_t wifi_dispatch(int argc, char *argv[])
{
    if (argc <= 0) {
        wifi_console.for_each_command(print_description, NULL);
        return ESP_OK;
    }
    return wifi_console.exec_command(argc, argv);
}
#endif // CONFIG_ENABLE_WIFI_AP || CONFIG_ENABLE_WIFI_STATION

esp_err_t wifi_register_commands()
{
#if CONFIG_ENABLE_WIFI_AP || CONFIG_ENABLE_WIFI_STATION
    static const command_t command = {
        .name = "wifi",
        .description = "Wi-Fi commands. Usage: matter esp wifi <wifi_command>.",
        .handler = wifi_dispatch,
    };

    static const command_t wifi_commands[] = {
        {
            .name = "connect",
            .description = "Connect to AP. Usage: matter esp wifi connect ssid psk.",
            .handler = wifi_connect_handler,
        },
#if CONFIG_ESP_MATTER_CONSOLE_USE_ESP_CONSOLE
        {
            .name = "status",
            .description = "Show configured SSID, country, DHCP and IP. Usage: matter esp wifi status.",
            .handler = wifi_status_handler,
        },
        {
            .name = "scan",
            .description = "Disconnect briefly, scan WiFi networks and reconnect. Usage: matter esp wifi scan.",
            .handler = wifi_scan_handler,
        },
#endif
    };
    wifi_console.register_commands(wifi_commands, sizeof(wifi_commands) / sizeof(command_t));

    return add_commands(&command, 1);
#else
    return ESP_OK;
#endif // CONFIG_ENABLE_WIFI_AP || CONFIG_ENABLE_WIFI_STATION
}
} // namespace console
} // namespace esp_matter
