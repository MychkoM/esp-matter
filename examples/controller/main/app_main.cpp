/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_netif.h>

#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_controller_client.h>
#include <esp_matter_controller_console.h>
#include <esp_matter_controller_pairing_command.h>
#include <esp_matter_controller_utils.h>
#include <esp_matter_ota.h>
#if CONFIG_OPENTHREAD_BORDER_ROUTER
#include <esp_openthread.h>
#include <esp_openthread_border_router.h>
#include <esp_openthread_lock.h>
#include <esp_ot_config.h>
#include <esp_spiffs.h>
#if defined(CONFIG_AUTO_UPDATE_RCP)
#include <esp_ot_rcp_update.h>
#include <esp_rcp_update.h>
#include <platform/ThreadStackManager.h>
#endif
#include <platform/ESP32/OpenthreadLauncher.h>
#endif // CONFIG_OPENTHREAD_BORDER_ROUTER
#if CONFIG_OPENTHREAD_BORDER_ROUTER && CONFIG_OPENTHREAD_RADIO_NATIVE
#include <esp_coexist.h>
#include <openthread/dataset_ftd.h>
#include <openthread/ip6.h>
#include <openthread/thread.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#endif
#include <common_macros.h>

#include <app/server/Server.h>
#include <credentials/FabricTable.h>

#ifdef CONFIG_CUSTOM_REVOKED_DAC_CHAIN_CHECK
#include <esp_matter_da_revocation_delegate.h>
#include <revocation_set/json_set_da_revocation_delegate.h>
extern const uint8_t revocation_set_json_start[] asm("_binary_revocation_set_json_start");
extern const uint8_t revocation_set_json_end[] asm("_binary_revocation_set_json_end");
static chip::Credentials::json_set_da_revocation_delegate s_custom_delegate((const char *)revocation_set_json_start,
                                                                            (const char *)revocation_set_json_end);
#endif

static const char *TAG = "app_main";
uint16_t switch_endpoint_id = 0;

#if CONFIG_OPENTHREAD_BORDER_ROUTER && CONFIG_OPENTHREAD_RADIO_NATIVE
static esp_err_t start_own_thread_network()
{
    if (!esp_openthread_lock_acquire(portMAX_DELAY)) {
        return ESP_ERR_TIMEOUT;
    }
    otInstance *instance = esp_openthread_get_instance();
    otError error = OT_ERROR_INVALID_STATE;
    if (instance) {
        error = OT_ERROR_NONE;
        if (!otDatasetIsCommissioned(instance)) {
            otOperationalDataset dataset = {};
            error = otDatasetCreateNewNetwork(instance, &dataset);
            if (error == OT_ERROR_NONE) {
                strcpy(dataset.mNetworkName.m8, "C6-Controller");
                error = otDatasetSetActive(instance, &dataset);
            }
        }
        if (error == OT_ERROR_NONE) {
            error = otIp6SetEnabled(instance, true);
        }
        if (error == OT_ERROR_NONE) {
            error = otThreadSetEnabled(instance, true);
        }
    }
    esp_openthread_lock_release();
    if (error != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "Failed to start own Thread network: %d", error);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Own Thread network started; operational dataset is stored in NVS");
    return ESP_OK;
}

static esp_err_t thread_handler(int argc, char **argv)
{
    if (argc == 1 && strcmp(argv[0], "info") == 0) {
        if (!esp_openthread_lock_acquire(portMAX_DELAY)) {
            return ESP_ERR_TIMEOUT;
        }
        otInstance *instance = esp_openthread_get_instance();
        otOperationalDataset dataset = {};
        otError error = instance ? otDatasetGetActive(instance, &dataset) : OT_ERROR_INVALID_STATE;
        if (error == OT_ERROR_NONE) {
            ESP_LOGI(TAG, "Thread network: %s, role: %s, channel: %u, PAN ID: 0x%04x",
                     dataset.mNetworkName.m8, otThreadDeviceRoleToString(otThreadGetDeviceRole(instance)),
                     dataset.mChannel, dataset.mPanId);
        }
        esp_openthread_lock_release();
        return error == OT_ERROR_NONE ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    if (argc != 3 || strcmp(argv[0], "pair") != 0) {
        ESP_LOGI(TAG, "Usage: matter esp thread info | matter esp thread pair <node-id> <setup-code>");
        return argc == 0 ? ESP_OK : ESP_ERR_INVALID_ARG;
    }
    char *end = nullptr;
    errno = 0;
    uint64_t node_id = strtoull(argv[1], &end, 0);
    if (errno || end == argv[1] || *end || argv[1][0] == '-' || node_id == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_matter::lock::ScopedChipStackLock chip_lock(portMAX_DELAY);
    if (!esp_openthread_lock_acquire(portMAX_DELAY)) {
        return ESP_ERR_TIMEOUT;
    }
    otInstance *instance = esp_openthread_get_instance();
    otOperationalDatasetTlvs dataset = {};
    otError error = instance ? otDatasetGetActiveTlvs(instance, &dataset) : OT_ERROR_INVALID_STATE;
    bool ready = instance && otThreadGetDeviceRole(instance) >= OT_DEVICE_ROLE_CHILD;
    esp_openthread_lock_release();
    if (error != OT_ERROR_NONE || !ready) {
        ESP_LOGE(TAG, "Thread network is not ready; check matter esp thread info");
        return ESP_ERR_INVALID_STATE;
    }
    return esp_matter::controller::pairing_command::pairing_code_thread(node_id, argv[2], dataset.mTlvs, dataset.mLength);
}
#endif

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::PublicEventTypes::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "Interface IP Address changed");
        break;
    case chip::DeviceLayer::DeviceEventType::kESPSystemEvent:
        if (event->Platform.ESPSystemEvent.Base == WIFI_EVENT &&
            event->Platform.ESPSystemEvent.Id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "Wi-Fi disconnected, reason=%u",
                 event->Platform.ESPSystemEvent.Data.WiFiStaDisconnected.reason);
        }
        if (event->Platform.ESPSystemEvent.Base == IP_EVENT &&
                event->Platform.ESPSystemEvent.Id == IP_EVENT_STA_GOT_IP) {
            const auto &ip_info = event->Platform.ESPSystemEvent.Data.IpGotIp.ip_info;
            ESP_LOGI(TAG, "Wi-Fi IPv4: " IPSTR ", gateway: " IPSTR, IP2STR(&ip_info.ip), IP2STR(&ip_info.gw));
#if CONFIG_OPENTHREAD_BORDER_ROUTER
            static bool sThreadBRInitialized = false;
            if (!sThreadBRInitialized) {
                esp_openthread_set_backbone_netif(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"));
                esp_openthread_lock_acquire(portMAX_DELAY);
                esp_err_t br_error = esp_openthread_border_router_init();
                esp_openthread_lock_release();
                if (br_error == ESP_OK) {
                    sThreadBRInitialized = true;
                } else {
                    ESP_LOGE(TAG, "Failed to initialize own Thread Border Router: %s", esp_err_to_name(br_error));
                }
            }
#endif
        }
        break;
    default:
        break;
    }
}

extern "C" void app_main()
{
    esp_err_t err = ESP_OK;

    /* Initialize the ESP NVS layer */
    nvs_flash_init();
#ifdef CONFIG_OPENTHREAD_BORDER_ROUTER
#ifdef CONFIG_AUTO_UPDATE_RCP
    esp_vfs_spiffs_conf_t rcp_fw_conf = {
        .base_path = "/rcp_fw", .partition_label = "rcp_fw", .max_files = 10, .format_if_mount_failed = false
    };
    if (ESP_OK != esp_vfs_spiffs_register(&rcp_fw_conf)) {
        ESP_LOGE(TAG, "Failed to mount rcp firmware storage");
        return;
    }
    esp_rcp_update_config_t rcp_update_config = ESP_OPENTHREAD_RCP_UPDATE_CONFIG();
    esp_rcp_update_init(&rcp_update_config);
    esp_ot_register_rcp_handler();
#endif
    /* Set OpenThread platform config */
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    set_openthread_platform_config(&config);
#endif // CONFIG_OPENTHREAD_BORDER_ROUTER
    /* Matter start */
    err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));

#if defined(CONFIG_OPENTHREAD_BORDER_ROUTER) && defined(CONFIG_AUTO_UPDATE_RCP)
    {
        esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
        using namespace chip::DeviceLayer;
        bool thread_was_enabled = ThreadStackMgr().IsThreadEnabled();
        if (thread_was_enabled) {
            ABORT_APP_ON_FAILURE(ThreadStackMgr().SetThreadEnabled(false) == CHIP_NO_ERROR,
                                 ESP_LOGE(TAG, "Failed to disable Thread before updating RCP"));
        }
        esp_ot_update_rcp_if_different();
        if (thread_was_enabled) {
            ABORT_APP_ON_FAILURE(ThreadStackMgr().SetThreadEnabled(true) == CHIP_NO_ERROR,
                                 ESP_LOGE(TAG, "Failed to enable Thread after updating RCP"));
        }
    }
#endif

#if CONFIG_ESP_MATTER_COMMISSIONER_ENABLE
    {
        esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
        esp_matter::controller::matter_controller_client::get_instance().init(112233, 1, 5580);
#ifdef CONFIG_CUSTOM_REVOKED_DAC_CHAIN_CHECK
        chip::Credentials::set_custom_da_revocation_delegate(&s_custom_delegate);
#endif
        esp_matter::controller::matter_controller_client::get_instance().setup_commissioner();
    }
#endif // CONFIG_ESP_MATTER_COMMISSIONER_ENABLE

#if CONFIG_OPENTHREAD_BORDER_ROUTER && CONFIG_OPENTHREAD_RADIO_NATIVE
    {
        esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
        ESP_ERROR_CHECK(start_own_thread_network());
#if CONFIG_ESP_COEX_SW_COEXIST_ENABLE
        // Native Thread shares the radio with Wi-Fi even when BLE is inactive.
        ESP_ERROR_CHECK(esp_coex_wifi_i154_enable());
#endif
    }
#endif

#if CONFIG_ENABLE_CHIP_SHELL
    ESP_ERROR_CHECK(esp_matter::console::diagnostics_register_commands());
    ESP_ERROR_CHECK(esp_matter::console::wifi_register_commands());
    ESP_ERROR_CHECK(esp_matter::console::factoryreset_register_commands());
#if CONFIG_ESP_MATTER_CONTROLLER_ENABLE
    ESP_ERROR_CHECK(esp_matter::console::controller_register_commands());
#endif
#if CONFIG_OPENTHREAD_BORDER_ROUTER
    ESP_ERROR_CHECK(esp_matter::console::otcli_register_commands());
#endif
#if CONFIG_OPENTHREAD_BORDER_ROUTER && CONFIG_OPENTHREAD_RADIO_NATIVE
    static const esp_matter::console::command_t thread_command = {
        .name = "thread",
        .description = "Own Thread network. Usage: matter esp thread info | pair <node-id> <setup-code>",
        .handler = thread_handler,
    };
    ESP_ERROR_CHECK(esp_matter::console::add_commands(&thread_command, 1));
#endif
    ESP_ERROR_CHECK(esp_matter::console::init());
#endif
}
