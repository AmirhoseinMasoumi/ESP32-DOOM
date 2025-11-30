// ESP32 DOOM Main Entry Point (prboom port)
// Based on doom-espidf by jkirsons

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"

// Streaming support - define DOOM_STREAMING to enable
#ifdef DOOM_STREAMING
#include "doom_stream.h"
// WiFi credentials - change these for your network!
#ifndef WIFI_SSID
#define WIFI_SSID "YourWiFiSSID"
#endif
#ifndef WIFI_PASSWORD  
#define WIFI_PASSWORD "YourWiFiPassword"
#endif
#endif

static const char *TAG = "DOOM";

extern void jsInit(void);
extern int doom_main(int argc, char const * const *argv);
extern void spi_lcd_init(void);

void doomEngineTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting DOOM engine...");
    
    // Print memory info
    ESP_LOGI(TAG, "Free DRAM: %lu bytes", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "Free PSRAM: %lu bytes", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    
    char const *argv[] = {"doom", "-cout", "ICWEFDA", NULL};
    doom_main(3, argv);
    
    ESP_LOGI(TAG, "DOOM engine exited");
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 DOOM Initializing...");
    
    // Check PSRAM
    if (esp_psram_get_size() > 0) {
        ESP_LOGI(TAG, "PSRAM size: %d MB", esp_psram_get_size() / (1024 * 1024));
    } else {
        ESP_LOGW(TAG, "No PSRAM detected!");
    }
    
#ifdef DOOM_STREAMING
    // Initialize streaming system
    ESP_LOGI(TAG, "Initializing DOOM streaming...");
    doom_stream_init();
    doom_stream_wifi_init(WIFI_SSID, WIFI_PASSWORD);
    // Give WiFi time to connect before starting DOOM
    vTaskDelay(pdMS_TO_TICKS(3000));
#endif

    // Initialize display
    spi_lcd_init();
    
    // Initialize input
    jsInit();
    
    // Start DOOM engine on core 0 with large stack (PSRAM)
    xTaskCreatePinnedToCore(
        &doomEngineTask, 
        "doomEngine", 
        32768,  // Stack size
        NULL, 
        5,      // Priority
        NULL, 
        0       // Core 0
    );
}
