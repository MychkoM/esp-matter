set(src_dirs)
if (CONFIG_ENABLE_CHIP_SHELL)
    list(APPEND src_dirs ".")
endif()

set(priv_req chip mbedtls esp_timer bt openthread esp_matter esp_netif lwip)

if (CONFIG_ESP_MATTER_CONSOLE_USE_ESP_CONSOLE)
    list(APPEND priv_req console)
endif()

if (NOT CONFIG_ESP_MATTER_ENABLE_DATA_MODEL)
    list(APPEND exclude_srcs_list "esp_matter_console_attribute.cpp")
endif()

idf_component_register(SRC_DIRS ${src_dirs}
                    INCLUDE_DIRS .
                    EXCLUDE_SRCS ${exclude_srcs_list}
                    PRIV_REQUIRES ${priv_req})

if (CONFIG_ENABLE_CHIP_SHELL)
    target_compile_options(${COMPONENT_LIB} PRIVATE "-Wstringop-truncation" "-Wstringop-overflow")
endif()
