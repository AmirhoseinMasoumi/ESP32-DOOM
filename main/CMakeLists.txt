# Main component - just app_main entry point
idf_component_register(
    SRCS "../prboom_main.c"
    INCLUDE_DIRS "."
    REQUIRES prboom prboom-esp32-compat esp_psram nvs_flash esp_wifi esp_event esp_netif
)

# Check if streaming is enabled (WiFi credentials passed)
if(DEFINED CACHE{WIFI_SSID} AND DEFINED CACHE{WIFI_PASSWORD})
    message(STATUS "main component: DOOM Streaming ENABLED")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        DOOM_STREAMING=1
        WIFI_SSID="${WIFI_SSID}"
        WIFI_PASSWORD="${WIFI_PASSWORD}"
    )
endif()
