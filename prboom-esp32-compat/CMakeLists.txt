# PrBoom ESP32 Compatibility Layer Component
idf_component_register(
    SRCS
        "i_main.c"
        "i_system.c"
        "i_video.c"
        "i_sound.c"
        "i_network.c"
        "spi_lcd.c"
        "gamepad.c"
        "psxcontroller.c"
        "doom_stream.c"
    INCLUDE_DIRS "include" "."
    REQUIRES 
        esp_timer 
        esp_partition 
        fatfs 
        driver
        esp_psram
        spi_flash
        vfs
        esp_wifi
        esp_http_server
        nvs_flash
        esp_netif
        esp_event
        lwip
    PRIV_REQUIRES
        prboom
)

# Compiler options - suppress warnings for legacy code
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Os
    -DHAVE_CONFIG_H
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-discarded-qualifiers
    -Wno-deprecated-declarations
)

# Fix deprecated FreeRTOS types and macros
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    portTICK_RATE_MS=portTICK_PERIOD_MS
    xQueueHandle=QueueHandle_t
)

# Check if streaming is enabled (WiFi credentials passed)
if(DEFINED CACHE{WIFI_SSID} AND DEFINED CACHE{WIFI_PASSWORD})
    message(STATUS "prboom-esp32-compat: DOOM Streaming ENABLED")
    target_compile_definitions(${COMPONENT_LIB} PUBLIC
        DOOM_STREAMING=1
        WIFI_SSID="${WIFI_SSID}"
        WIFI_PASSWORD="${WIFI_PASSWORD}"
    )
else()
    message(STATUS "prboom-esp32-compat: DOOM Streaming DISABLED (no WiFi credentials)")
endif()
