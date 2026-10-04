#include <sdkconfig.h>

#if CONFIG_ESP_MATTER_CONSOLE_NETWORK

#include <esp_matter_console.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <memory>

namespace esp_matter {
namespace console {

static const char *TAG = "console_network";
static char log_buffer[8192];
static uint64_t log_end;
static portMUX_TYPE log_lock = portMUX_INITIALIZER_UNLOCKED;
static vprintf_like_t original_logger = vprintf;

void network_write(const char *data, size_t length)
{
    portENTER_CRITICAL(&log_lock);
    for (size_t index = 0; index < length; ++index) {
        log_buffer[log_end++ % sizeof(log_buffer)] = data[index];
    }
    portEXIT_CRITICAL(&log_lock);
}

static int capture_log(const char *format, va_list arguments)
{
    char buffer[512];
    const char *message = buffer;
    std::unique_ptr<char, decltype(&free)> full_message(nullptr, free);
    va_list copy;
    va_copy(copy, arguments);
    int length = vsnprintf(buffer, sizeof(buffer), format, copy);
    va_end(copy);
    if (length > 0) {
        size_t count = static_cast<size_t>(length);
        if (count >= sizeof(buffer)) {
            full_message.reset(static_cast<char *>(malloc(count + 1)));
            if (full_message) {
                message = full_message.get();
                va_copy(copy, arguments);
                vsnprintf(full_message.get(), count + 1, format, copy);
                va_end(copy);
            } else {
                message = "[log dropped: insufficient memory]\n";
                count = strlen(message);
            }
        }
        char level = 0;
        unsigned long long timestamp = 0;
        char tag[64] = {};
        int prefix_length = 0;
        size_t start = 0;
        if (sscanf(message, "%c (%llu) %63[^:]:%n", &level, &timestamp, tag, &prefix_length) == 3 &&
            prefix_length > 0 && strchr("EWIDV", level)) {
            if (strcmp(tag, "ROUTE_HOOK") == 0) {
                return original_logger(format, arguments);
            }
            start = static_cast<size_t>(prefix_length);
            if (message[start] == ' ') {
                ++start;
            }
        }
        network_write(message + start, count - start);
    }
    return original_logger(format, arguments);
}

static esp_err_t terminal_info(int argc, char **argv)
{
    if (argc != 1 || strcmp(argv[0], "info") != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_netif_ip_info_t address = {};
    esp_netif_t *station = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (station) {
        esp_netif_get_ip_info(station, &address);
    }
    ESP_LOGI(TAG, "Telnet console: " IPSTR ":%d", IP2STR(&address.ip), CONFIG_ESP_MATTER_CONSOLE_NETWORK_PORT);
    return ESP_OK;
}

static bool send_bytes(int peer, const char *bytes, size_t length)
{
    int64_t deadline = esp_timer_get_time() + 5000000;
    size_t sent = 0;
    while (sent < length && esp_timer_get_time() < deadline) {
        ssize_t result = send(peer, bytes + sent, length - sent, 0);
        if (result > 0) {
            sent += result;
        } else if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            return false;
        }
    }
    return sent == length;
}

static bool send_text(int peer, const char *text, size_t length, bool previous_was_cr = false)
{
    size_t start = 0;
    for (size_t index = 0; index < length; ++index) {
        unsigned char value = text[index];
        bool newline = value == '\n' && (index == 0 ? !previous_was_cr : text[index - 1] != '\r');
        if (value == 255 || newline) {
            if (!send_bytes(peer, text + start, index - start)) {
                return false;
            }
            const char *escaped = value == 255 ? "\xff\xff" : "\r\n";
            if (!send_bytes(peer, escaped, 2)) {
                return false;
            }
            start = index + 1;
        }
    }
    return send_bytes(peer, text + start, length - start);
}

static bool send_text(int peer, const char *text)
{
    return send_text(peer, text, strlen(text));
}

static bool send_logs(int peer, uint64_t &cursor)
{
    char chunk[512];
    portENTER_CRITICAL(&log_lock);
    uint64_t end = log_end;
    portEXIT_CRITICAL(&log_lock);
    while (cursor < end) {
        size_t count = 0;
        bool dropped = false;
        bool previous_was_cr = false;
        portENTER_CRITICAL(&log_lock);
        uint64_t first = log_end > sizeof(log_buffer) ? log_end - sizeof(log_buffer) : 0;
        if (cursor < first) {
            cursor = first;
            dropped = true;
        }
        if (cursor > first) {
            previous_was_cr = log_buffer[(cursor - 1) % sizeof(log_buffer)] == '\r';
        }
        while (cursor < end && count < sizeof(chunk)) {
            chunk[count++] = log_buffer[cursor++ % sizeof(log_buffer)];
        }
        portEXIT_CRITICAL(&log_lock);
        if (dropped && !send_text(peer, "\r\n[older logs dropped]\r\n")) {
            return false;
        }
        if (!send_text(peer, chunk, count, previous_was_cr)) {
            return false;
        }
    }
    return true;
}

enum class telnet_state { data, command, option, subnegotiation, subnegotiation_command };

static void serve_session(int peer)
{
    const char negotiation[] = {static_cast<char>(255), static_cast<char>(251), 3,
                                static_cast<char>(255), static_cast<char>(251), 1};
    if (!send_bytes(peer, negotiation, sizeof(negotiation)) ||
        !send_text(peer, "ESP Matter Telnet console\r\nmatter> ")) {
        return;
    }
    char line[CONFIG_CHIP_SHELL_CMD_LINE_BUF_MAX_LENGTH];
    size_t used = 0;
    bool overflow = false;
    bool local_sga = true;
    bool local_echo = true;
    bool remote_sga = false;
    bool pending_cr = false;
    telnet_state state = telnet_state::data;
    unsigned char verb = 0;
    uint64_t cursor;
    portENTER_CRITICAL(&log_lock);
    cursor = log_end;
    portEXIT_CRITICAL(&log_lock);
    int64_t last_input = esp_timer_get_time();
    while (esp_timer_get_time() - last_input < 900000000LL) {
        char input[128];
        ssize_t count = recv(peer, input, sizeof(input), 0);
        if (count == 0 || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            return;
        }
        if (count > 0) {
            last_input = esp_timer_get_time();
        }
        for (ssize_t index = 0; index < count; ++index) {
            unsigned char value = input[index];
            if (state == telnet_state::subnegotiation) {
                if (value == 255) {
                    state = telnet_state::subnegotiation_command;
                }
                continue;
            }
            if (state == telnet_state::subnegotiation_command) {
                state = value == 240 ? telnet_state::data : telnet_state::subnegotiation;
                continue;
            }
            if (state == telnet_state::option) {
                unsigned char reply = 0;
                if (verb == 253) {
                    if (value == 1) {
                        if (!local_echo) {
                            local_echo = true;
                            reply = 251;
                        }
                    } else if (value != 3) {
                        reply = 252;
                    } else if (!local_sga) {
                        local_sga = true;
                        reply = 251;
                    }
                } else if (verb == 251) {
                    if (value != 3) {
                        reply = 254;
                    } else if (!remote_sga) {
                        remote_sga = true;
                        reply = 253;
                    }
                } else if (verb == 254 && value == 1 && local_echo) {
                    local_echo = false;
                    reply = 252;
                } else if (verb == 254 && value == 3 && local_sga) {
                    local_sga = false;
                    reply = 252;
                } else if (verb == 252 && value == 3) {
                    remote_sga = false;
                }
                state = telnet_state::data;
                if (reply) {
                    const char response[] = {static_cast<char>(255), static_cast<char>(reply), static_cast<char>(value)};
                    if (!send_bytes(peer, response, sizeof(response))) {
                        return;
                    }
                }
                continue;
            }
            if (state == telnet_state::command) {
                state = telnet_state::data;
                if (value >= 251 && value <= 254) {
                    verb = value;
                    state = telnet_state::option;
                    continue;
                }
                if (value == 250) {
                    state = telnet_state::subnegotiation;
                    continue;
                }
                if (value == 244) {
                    return;
                } else if (value == 248) {
                    if (local_echo) {
                        for (size_t erased = 0; erased < used; ++erased) {
                            if (!send_text(peer, "\b \b")) {
                                return;
                            }
                        }
                    }
                    used = 0;
                    overflow = false;
                    continue;
                } else if (value == 247) {
                    value = 127;
                } else if (value != 255) {
                    continue;
                }
            } else if (value == 255) {
                state = telnet_state::command;
                continue;
            }
            if (pending_cr && (value == '\n' || value == 0)) {
                pending_cr = false;
                continue;
            }
            pending_cr = false;
            if (value == 0) {
                continue;
            }
            if (value == 3 || value == 4) {
                return;
            }
            if (value == '\r' || value == '\n') {
                pending_cr = value == '\r';
                if (!send_text(peer, "\r\n")) {
                    return;
                }
                if (used == 0 && !overflow) {
                    if (!send_text(peer, "matter> ")) {
                        return;
                    }
                    continue;
                }
                line[used] = 0;
                esp_err_t result = overflow ? ESP_ERR_INVALID_SIZE : execute_command_line(line);
                if (!send_logs(peer, cursor)) {
                    return;
                }
                char response[96];
                snprintf(response, sizeof(response), "\r\nRESULT %s\r\nmatter> ", esp_err_to_name(result));
                if (!send_text(peer, response)) {
                    return;
                }
                used = 0;
                overflow = false;
            } else if (value == 8 || value == 127) {
                if (used > 0 && !overflow) {
                    --used;
                    if (local_echo && !send_text(peer, "\b \b")) {
                        return;
                    }
                }
            } else if (!overflow) {
                if (used + 1 >= sizeof(line)) {
                    overflow = true;
                } else {
                    line[used++] = value;
                    if (local_echo && !send_text(peer, reinterpret_cast<const char *>(&value), 1)) {
                        return;
                    }
                }
            }
        }
        if (!send_logs(peer, cursor)) {
            return;
        }
        if (count < 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

static void network_task(void *)
{
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(CONFIG_ESP_MATTER_CONSOLE_NETWORK_PORT);
    if (listener < 0 || bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 ||
        listen(listener, 1) != 0) {
        ESP_LOGE(TAG, "Could not listen on Telnet port: %d", errno);
        if (listener >= 0) {
            close(listener);
        }
        vTaskDelete(nullptr);
        return;
    }
    original_logger = esp_log_set_vprintf(capture_log);
    ESP_LOGW(TAG, "Test-only Telnet on port %d: no authentication or encryption", CONFIG_ESP_MATTER_CONSOLE_NETWORK_PORT);
    for (;;) {
        int peer = accept(listener, nullptr, nullptr);
        if (peer < 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (fcntl(peer, F_SETFL, O_NONBLOCK) == 0) {
            serve_session(peer);
        }
        close(peer);
    }
}

esp_err_t network_start()
{
    static const command_t commands[] = {
        {"terminal", "Telnet console address: terminal info", terminal_info},
    };
    esp_err_t result = add_commands(commands, 1);
    if (result != ESP_OK) {
        return result;
    }
    return xTaskCreate(network_task, "console_net", 8192, nullptr, 4, nullptr) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

} // namespace console
} // namespace esp_matter

#endif