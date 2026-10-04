set(SRC_DIRS_LIST ".")
if (CONFIG_CUSTOM_REVOKED_DAC_CHAIN_CHECK)
    list(APPEND SRC_DIRS_LIST "revocation_set")
endif()

idf_component_register(SRC_DIRS          ${SRC_DIRS_LIST}
                       INCLUDE_DIRS      "."
                       PRIV_INCLUDE_DIRS  "." "${ESP_MATTER_PATH}/examples/common/utils"
                       LDFRAGMENTS "linker.lf")

if(CONFIG_SPIFFS_ATTESTATION_TRUST_STORE)
    if(CONFIG_IDF_TARGET_ESP32C6)
        set(paa_cert_dir "${CMAKE_CURRENT_BINARY_DIR}/paa_cert")
        file(MAKE_DIRECTORY "${paa_cert_dir}")
        file(GLOB paa_cert_files CONFIGURE_DEPENDS
             "${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip/credentials/development/paa-root-certs/*.der")
        foreach(paa_cert_file IN LISTS paa_cert_files)
            file(SHA256 "${paa_cert_file}" paa_cert_hash)
            string(SUBSTRING "${paa_cert_hash}" 0 24 paa_cert_name)
            configure_file("${paa_cert_file}" "${paa_cert_dir}/${paa_cert_name}.der" COPYONLY)
        endforeach()
    else()
        set(paa_cert_dir "${CMAKE_SOURCE_DIR}/paa_cert")
    endif()
    spiffs_create_partition_image(paa_cert "${paa_cert_dir}" FLASH_IN_PROJECT)
endif()

target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
