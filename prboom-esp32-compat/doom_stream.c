// DOOM Display Streaming - Server-Sent Events (SSE) with RLE compression
// High-performance streaming with 8-bit indexed frames + RLE
// Uses SSE for low-latency push to browser
// Now with keyboard input support via HTTP and Serial!

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_heap_caps.h"
#include "driver/uart.h"

// DOOM headers for input
#include "doomdef.h"
#include "doomtype.h"
#include "d_event.h"
#include "d_main.h"
#include "g_game.h"

static const char *TAG = "DOOM_STREAM";

// Serial key input configuration
#define SERIAL_KEY_UART UART_NUM_0
#define SERIAL_KEY_BUF_SIZE 256

// Stream configuration - full resolution
#define STREAM_WIDTH  320
#define STREAM_HEIGHT 200
#define FRAME_PIXELS  (STREAM_WIDTH * STREAM_HEIGHT)
#define RLE_BUFFER_SIZE (FRAME_PIXELS + 4096)

// Frame buffers
static uint8_t *frame_buffer = NULL;
static uint8_t *rle_buffer = NULL;
static uint8_t current_palette[768];
static bool palette_changed = true;
static SemaphoreHandle_t frame_mutex = NULL;
static volatile uint32_t frame_count = 0;

// HTTP server
static httpd_handle_t stream_httpd = NULL;

// Stats
static uint32_t total_bytes = 0;
static uint32_t frames_sent = 0;

// Key queue for async handling - avoid blocking HTTP server
#define KEY_QUEUE_SIZE 32
typedef struct {
    int doom_key;
    int state;
} queued_key_t;
static queued_key_t key_queue[KEY_QUEUE_SIZE];
static volatile int key_queue_head = 0;
static volatile int key_queue_tail = 0;

// Add key to queue (used by both HTTP and Serial handlers)
static void queue_key(int doom_key, int state) {
    int next_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
    if (next_head != key_queue_tail) {  // Queue not full
        key_queue[key_queue_head].doom_key = doom_key;
        key_queue[key_queue_head].state = state;
        key_queue_head = next_head;
    }
}

// Serial key reading state machine
static uint8_t serial_buf[3];
static int serial_buf_idx = 0;
static uint32_t serial_last_byte_time = 0;

// Read keys from serial port (Protocol: 'K' <keycode> <state>)
// Uses non-blocking read from stdin (UART0 via VFS)
void doom_stream_read_serial_keys(void) {
    uint8_t byte;
    uint32_t now = xTaskGetTickCount();
    
    // Reset state machine if timeout (100ms without data)
    if (serial_buf_idx > 0 && (now - serial_last_byte_time) > pdMS_TO_TICKS(100)) {
        serial_buf_idx = 0;
    }
    
    // Non-blocking read using uart driver directly
    while (uart_read_bytes(UART_NUM_0, &byte, 1, 0) > 0) {
        serial_last_byte_time = now;
        
        if (serial_buf_idx == 0) {
            // Looking for 'K' start byte
            if (byte == 'K') {
                serial_buf[0] = byte;
                serial_buf_idx = 1;
            }
        } else {
            // Collecting keycode and state
            serial_buf[serial_buf_idx++] = byte;
            
            if (serial_buf_idx >= 3) {
                // Complete packet: K <keycode> <state>
                int doom_key = serial_buf[1];
                int state = serial_buf[2];
                queue_key(doom_key, state);
                // Reduced logging - only log occasionally
                static int key_count = 0;
                if (++key_count % 10 == 1) {
                    ESP_LOGI(TAG, "KEY: %d %s (count=%d)", doom_key, state ? "DN" : "UP", key_count);
                }
                serial_buf_idx = 0;
            }
        }
    }
}

// Process queued keys from main DOOM loop
void doom_stream_process_keys(void) {
    // First read any pending serial keys
    doom_stream_read_serial_keys();
    
    // Then process all queued keys
    while (key_queue_head != key_queue_tail) {
        int idx = key_queue_tail;
        queued_key_t k = key_queue[idx];
        key_queue_tail = (idx + 1) % KEY_QUEUE_SIZE;
        
        event_t ev;
        ev.type = k.state ? ev_keydown : ev_keyup;
        ev.data1 = k.doom_key;
        ev.data2 = 0;
        ev.data3 = 0;
        D_PostEvent(&ev);
        ESP_LOGW(TAG, "POST: key=%d(0x%x) %s", k.doom_key, k.doom_key, k.state ? "DOWN" : "UP");
    }
}

// RLE encode: [count][value] pairs
static size_t rle_encode(const uint8_t *src, size_t len, uint8_t *dst, size_t max) {
    size_t di = 0, si = 0;
    while (si < len && di < max - 3) {
        uint8_t v = src[si];
        size_t run = 1;
        while (si + run < len && src[si + run] == v && run < 255) run++;
        dst[di++] = (uint8_t)run;
        dst[di++] = v;
        si += run;
    }
    dst[di++] = 0;
    return di;
}

// Base64 encode for SSE binary data
static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static size_t base64_encode(const uint8_t *src, size_t len, char *dst) {
    size_t di = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (src[i] << 16);
        if (i + 1 < len) n |= (src[i + 1] << 8);
        if (i + 2 < len) n |= src[i + 2];
        dst[di++] = b64[(n >> 18) & 0x3F];
        dst[di++] = b64[(n >> 12) & 0x3F];
        dst[di++] = (i + 1 < len) ? b64[(n >> 6) & 0x3F] : '=';
        dst[di++] = (i + 2 < len) ? b64[n & 0x3F] : '=';
    }
    dst[di] = 0;
    return di;
}

// Map JavaScript keyCode to DOOM key (using KEYD_* constants from doomdef.h)
static int web_keycode_to_doom(int keycode) {
    switch (keycode) {
        // Arrow keys -> DOOM arrow keys
        case 38: return KEYD_UPARROW;      // ArrowUp -> forward
        case 40: return KEYD_DOWNARROW;    // ArrowDown -> backward
        case 37: return KEYD_LEFTARROW;    // ArrowLeft -> turn left
        case 39: return KEYD_RIGHTARROW;   // ArrowRight -> turn right
        
        // WASD -> same as arrows
        case 87: return KEYD_UPARROW;      // W -> forward
        case 83: return KEYD_DOWNARROW;    // S -> backward
        case 65: return ',';               // A -> strafe left (default key)
        case 68: return '.';               // D -> strafe right (default key)
        
        // Action keys
        case 32: return ' ';               // Space -> use/open (default key)
        case 17: return KEYD_RCTRL;        // Ctrl -> fire
        case 16: return KEYD_RSHIFT;       // Shift -> run
        case 13: return KEYD_ENTER;        // Enter
        case 27: return KEYD_ESCAPE;       // Escape
        case 9:  return KEYD_TAB;          // Tab -> automap
        
        // Weapon keys 1-7 (ASCII codes)
        case 49: return '1';
        case 50: return '2';
        case 51: return '3';
        case 52: return '4';
        case 53: return '5';
        case 54: return '6';
        case 55: return '7';
        
        // Additional strafe keys
        case 81: return ',';               // Q -> strafe left
        case 69: return '.';               // E -> strafe right
        
        // Menu navigation
        case 80: return KEYD_PAUSE;        // P -> pause
        case 8:  return KEYD_BACKSPACE;    // Backspace
        
        default: return 0;
    }
}

void doom_stream_init(void) {
    frame_buffer = heap_caps_malloc(FRAME_PIXELS, MALLOC_CAP_SPIRAM);
    rle_buffer = heap_caps_malloc(RLE_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (!frame_buffer || !rle_buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffers!");
        return;
    }
    memset(frame_buffer, 0, FRAME_PIXELS);
    frame_mutex = xSemaphoreCreateMutex();
    
    // Initialize UART0 for serial key input (shared with console)
    // Only install driver if not already installed
    if (!uart_is_driver_installed(UART_NUM_0)) {
        uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };
        uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
        uart_param_config(UART_NUM_0, &uart_config);
        ESP_LOGI(TAG, "UART0 driver installed for serial keys");
    } else {
        ESP_LOGI(TAG, "UART0 driver already installed");
    }
    
    ESP_LOGI(TAG, "Stream initialized (%dx%d + RLE)", STREAM_WIDTH, STREAM_HEIGHT);
}

void doom_stream_set_palette(const uint8_t *palette) {
    if (!palette || !frame_mutex) return;
    if (xSemaphoreTake(frame_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (memcmp(current_palette, palette, 768) != 0) {
            memcpy(current_palette, palette, 768);
            palette_changed = true;
        }
        xSemaphoreGive(frame_mutex);
    }
}

void doom_stream_update_frame(const uint8_t *pixels, int width, int height) {
    if (!frame_buffer || !pixels || !frame_mutex) return;
    if (xSemaphoreTake(frame_mutex, 0) == pdTRUE) {
        int h = (height > STREAM_HEIGHT) ? STREAM_HEIGHT : height;
        int w = (width > STREAM_WIDTH) ? STREAM_WIDTH : width;
        for (int y = 0; y < h; y++)
            memcpy(&frame_buffer[y * STREAM_WIDTH], &pixels[y * width], w);
        frame_count++;
        xSemaphoreGive(frame_mutex);
    }
}

// HTML page with SSE client
static const char index_html[] = 
"<!DOCTYPE html><html><head><title>ESP32 DOOM</title>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box;}"
"body{background:#000;color:#0f0;font-family:monospace;text-align:center;padding:10px;}"
"h1{color:#f00;text-shadow:0 0 10px #f00;margin:10px;font-size:clamp(1em,4vw,1.5em);}"
"canvas{image-rendering:pixelated;border:2px solid #333;width:min(95vw,640px);height:auto;"
"aspect-ratio:320/200;display:block;margin:10px auto;}"
"#stats{color:#0f0;font-size:12px;margin:5px;}"
"#status{color:#ff0;margin:5px;}"
".btn{background:#222;color:#0f0;border:1px solid #0f0;padding:8px 16px;margin:5px;"
"cursor:pointer;font-family:monospace;}"
".btn:hover{background:#0f0;color:#000;}"
"</style></head><body>"
"<h1>&#x1F47E; ESP32 DOOM</h1>"
"<div id='status'>Connecting...</div>"
"<canvas id='c' width='320' height='200'></canvas>"
"<div id='stats'>Waiting...</div>"
"<button class='btn' onclick='C.requestFullscreen?.()'>Fullscreen</button>"
"<div style='margin-top:10px;color:#888;font-size:11px;'>"
"Arrows:Move | Ctrl:Fire | Space:Use | Shift:Run | Enter:Select | Esc:Menu | 1-7:Weapons</div>"
"<div id='keylog' style='margin-top:10px;color:#0ff;font-size:12px;'>Keys: (none)</div>"
"<script>"
"const C=document.getElementById('c'),X=C.getContext('2d'),KL=document.getElementById('keylog');"
"let lastKeys=[];"
"function K(k,s){"
"console.log('Sending key:',k,'state:',s);"
"lastKeys.unshift(k+(s?'D':'U'));"
"if(lastKeys.length>10)lastKeys.pop();"
"KL.textContent='Keys: '+lastKeys.join(' ');"
"fetch('/key?k='+k+'&s='+s).then(r=>r.text()).then(t=>console.log('Key response:',t)).catch(e=>console.error('Key error:',e));"
"}"
"const gameKeys=[37,38,39,40,13,32,17,16,27,49,50,51,52,53,54,55];"
"document.addEventListener('keydown',e=>{if(gameKeys.includes(e.keyCode)){K(e.keyCode,1);e.preventDefault();}});"
"document.addEventListener('keyup',e=>{if(gameKeys.includes(e.keyCode)){K(e.keyCode,0);}});"
"C.addEventListener('click',()=>C.focus());"
"C.tabIndex=0;"
"window.onload=()=>{C.focus();console.log('Page loaded, canvas focused');};"
"const S=document.getElementById('status'),T=document.getElementById('stats');"
"const I=X.createImageData(320,200);"
"let P=new Uint8Array(768),fc=0,bc=0,lt=Date.now();"
"function b64d(s){const L='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';"
"let b=[],p=0;for(let i=0;i<s.length;i++){let c=L.indexOf(s[i]);if(c>=0){p=(p<<6)|c;"
"if((i&3)===3){b.push((p>>16)&255,(p>>8)&255,p&255);p=0;}}}return new Uint8Array(b);}"
"function connect(){"
"S.textContent='Connecting...';S.style.color='#ff0';"
"const es=new EventSource('/stream');"
"es.onopen=()=>{S.textContent='Connected!';S.style.color='#0f0';};"
"es.onerror=()=>{S.textContent='Error - Reconnecting...';S.style.color='#f00';es.close();setTimeout(connect,2000);};"
"es.onmessage=e=>{"
"const d=b64d(e.data),t=d[0];"
"if(t===80){P=d.slice(1);return;}"
"if(t===70){"
"const r=d.slice(1),p=I.data;let pi=0,ri=0;"
"while(ri<r.length){const n=r[ri++];if(n===0)break;const v=r[ri++];"
"const R=P[v*3],G=P[v*3+1],B=P[v*3+2];"
"for(let j=0;j<n&&pi<64000;j++,pi++){const o=pi*4;p[o]=R;p[o+1]=G;p[o+2]=B;p[o+3]=255;}}"
"X.putImageData(I,0,0);fc++;bc+=e.data.length;"
"const now=Date.now();if(now-lt>=1000){"
"T.textContent='FPS:'+fc+' | '+(bc/1024).toFixed(1)+'KB/s';fc=0;bc=0;lt=now;}}};}"
"connect();</script></body></html>";

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html, sizeof(index_html) - 1);
}

// SSE stream handler
static esp_err_t stream_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    // Allocate base64 buffer (4/3 of max RLE size + overhead)
    size_t b64_size = ((RLE_BUFFER_SIZE + 2) / 3) * 4 + 32;
    char *b64_buf = heap_caps_malloc(b64_size, MALLOC_CAP_SPIRAM);
    uint8_t *send_buf = heap_caps_malloc(RLE_BUFFER_SIZE + 16, MALLOC_CAP_SPIRAM);
    
    if (!b64_buf || !send_buf) {
        if (b64_buf) free(b64_buf);
        if (send_buf) free(send_buf);
        return ESP_FAIL;
    }
    
    uint32_t last_frame = 0;
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "SSE client connected");
    
    while (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(40));  // ~25 FPS max
        taskYIELD();  // Allow other tasks/requests to run
        
        if (frame_count == last_frame) continue;
        
        if (xSemaphoreTake(frame_mutex, pdMS_TO_TICKS(10)) != pdTRUE) continue;
        
        // Send palette if changed
        if (palette_changed) {
            send_buf[0] = 'P';
            memcpy(&send_buf[1], current_palette, 768);
            size_t b64_len = base64_encode(send_buf, 769, b64_buf);
            palette_changed = false;
            xSemaphoreGive(frame_mutex);
            
            // SSE format: "data: <base64>\n\n"
            ret = httpd_resp_send_chunk(req, "data:", 5);
            if (ret == ESP_OK) ret = httpd_resp_send_chunk(req, b64_buf, b64_len);
            if (ret == ESP_OK) ret = httpd_resp_send_chunk(req, "\n\n", 2);
            
            if (ret != ESP_OK) break;
            vTaskDelay(pdMS_TO_TICKS(10));
            if (xSemaphoreTake(frame_mutex, pdMS_TO_TICKS(10)) != pdTRUE) continue;
        }
        
        // RLE encode frame
        size_t rle_len = rle_encode(frame_buffer, FRAME_PIXELS, rle_buffer, RLE_BUFFER_SIZE);
        send_buf[0] = 'F';
        memcpy(&send_buf[1], rle_buffer, rle_len);
        size_t total = 1 + rle_len;
        last_frame = frame_count;
        xSemaphoreGive(frame_mutex);
        
        // Base64 encode
        size_t b64_len = base64_encode(send_buf, total, b64_buf);
        
        // Send SSE event
        ret = httpd_resp_send_chunk(req, "data:", 5);
        if (ret == ESP_OK) ret = httpd_resp_send_chunk(req, b64_buf, b64_len);
        if (ret == ESP_OK) ret = httpd_resp_send_chunk(req, "\n\n", 2);
        
        total_bytes += b64_len;
        frames_sent++;
        
        if (frames_sent % 150 == 0) {
            ESP_LOGI(TAG, "Sent %lu frames, RLE avg: %lu bytes", 
                     (unsigned long)frames_sent, 
                     (unsigned long)(total_bytes / frames_sent));
        }
    }
    
    free(b64_buf);
    free(send_buf);
    ESP_LOGI(TAG, "SSE client disconnected");
    return ret;
}

static esp_err_t status_handler(httpd_req_t *req) {
    char buf[256];
    snprintf(buf, sizeof(buf), 
        "{\"frames\":%lu,\"sent\":%lu,\"heap\":%lu}",
        (unsigned long)frame_count, (unsigned long)frames_sent,
        (unsigned long)esp_get_free_heap_size());
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, strlen(buf));
}

// Key input handler for web controls - FAST, just queue and return
static esp_err_t key_handler(httpd_req_t *req) {
    char query[64] = {0};
    int keycode = 0;
    int state = 0;
    
    esp_err_t qret = httpd_req_get_url_query_str(req, query, sizeof(query));
    
    if (qret == ESP_OK) {
        char param[16];
        if (httpd_query_key_value(query, "k", param, sizeof(param)) == ESP_OK) {
            keycode = atoi(param);
        }
        if (httpd_query_key_value(query, "s", param, sizeof(param)) == ESP_OK) {
            state = atoi(param);
        }
    }
    
    int doom_key = web_keycode_to_doom(keycode);
    
    if (doom_key != 0) {
        queue_key(doom_key, state);
        ESP_LOGI(TAG, "HTTP KEY QUEUED: web=%d doom=%d state=%d", keycode, doom_key, state);
    }
    
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "ok", 2);
}

static void start_server(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 16384;
    cfg.max_uri_handlers = 8;
    cfg.max_open_sockets = 7;  // Increased for SSE + key requests
    cfg.lru_purge_enable = true;
    cfg.recv_wait_timeout = 5;  // Shorter timeout
    cfg.send_wait_timeout = 5;
    cfg.backlog_conn = 5;  // Allow more pending connections
    cfg.core_id = 0;  // Run on core 0 (DOOM runs on core 1)
    
    ESP_LOGI(TAG, "Starting server on port %d", cfg.server_port);
    
    if (httpd_start(&stream_httpd, &cfg) == ESP_OK) {
        httpd_uri_t uri_idx = {.uri="/", .method=HTTP_GET, .handler=index_handler};
        httpd_uri_t uri_str = {.uri="/stream", .method=HTTP_GET, .handler=stream_handler};
        httpd_uri_t uri_sta = {.uri="/status", .method=HTTP_GET, .handler=status_handler};
        httpd_uri_t uri_key = {.uri="/key", .method=HTTP_GET, .handler=key_handler};
        httpd_register_uri_handler(stream_httpd, &uri_idx);
        httpd_register_uri_handler(stream_httpd, &uri_str);
        httpd_register_uri_handler(stream_httpd, &uri_sta);
        httpd_register_uri_handler(stream_httpd, &uri_key);
        ESP_LOGI(TAG, "Server started - open http://<IP>/");
    }
}

static volatile bool wifi_connected = false;

static void wifi_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) esp_wifi_connect();
        else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_connected = false;
            ESP_LOGW(TAG, "WiFi disconnected");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_wifi_connect();
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        wifi_connected = true;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&e->ip_info.ip));
        if (!stream_httpd) start_server();
    }
}

void doom_stream_wifi_init(const char *ssid, const char *password) {
    ESP_LOGI(TAG, "Init WiFi...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_handler, NULL, NULL);
    esp_wifi_set_mode(WIFI_MODE_STA);
    wifi_config_t wcfg = {.sta={.threshold.authmode=WIFI_AUTH_WPA2_PSK}};
    strncpy((char*)wcfg.sta.ssid, ssid, sizeof(wcfg.sta.ssid)-1);
    strncpy((char*)wcfg.sta.password, password, sizeof(wcfg.sta.password)-1);
    ESP_LOGI(TAG, "Connecting to: %s", ssid);
    esp_wifi_set_config(WIFI_IF_STA, &wcfg);
    esp_wifi_start();
    for (int i = 0; i < 20 && !wifi_connected; i++) vTaskDelay(pdMS_TO_TICKS(500));
    if (wifi_connected) {
        ESP_LOGI(TAG, "WiFi connected!");
    } else {
        ESP_LOGW(TAG, "WiFi connecting...");
    }
}

void doom_stream_start(void) { ESP_LOGI(TAG, "Streaming ready"); }
bool doom_stream_is_enabled(void) { return frame_buffer && stream_httpd; }
