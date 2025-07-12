/**
 * Web服务器实现
 * 功能: HTTP服务器和API接口
 */

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "cJSON.h"

// WebSocket支持检查 - 简化版本，暂时禁用WebSocket
#define WEBSOCKET_SUPPORTED 1

// 如果需要启用WebSocket，请确保ESP-IDF版本支持并取消下面的注释
// #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
// #define WEBSOCKET_SUPPORTED 1
// #else
// #define WEBSOCKET_SUPPORTED 0
// #endif

#include "web_server.h"
#include "kvm_controller.h"
#include "wifi_manager.h"
#include "uart_comm.h"

static const char *TAG = "WEB_SERVER";

// 服务器句柄
static httpd_handle_t server = NULL;

// WebSocket功能已禁用，删除相关变量

// 嵌入的网页文件
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[]   asm("_binary_style_css_end");
extern const uint8_t script_js_start[] asm("_binary_script_js_start");
extern const uint8_t script_js_end[]   asm("_binary_script_js_end");
extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[]   asm("_binary_favicon_ico_end");

/**
 * 发送HTTP响应
 */
static esp_err_t send_response(httpd_req_t *req, const char *data, size_t len, const char *content_type)
{
    httpd_resp_set_type(req, content_type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    return httpd_resp_send(req, data, len);
}

/**
 * OPTIONS请求处理器（用于CORS预检）
 */
static esp_err_t options_handler(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_send(req, "", 0);
    return ESP_OK;
}

// WebSocket处理器已删除，功能完全禁用

/**
 * 向所有WebSocket客户端广播消息
 */
esp_err_t web_server_broadcast_ws_message(const char *message)
{
    if (server == NULL || message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // WebSocket功能已禁用，只记录日志
    ESP_LOGD(TAG, "WebSocket已禁用，跳过消息广播: %s", message);
    return ESP_OK;
}

/**
 * 主页处理器
 */
static esp_err_t index_handler(httpd_req_t *req)
{
    const size_t index_html_len = index_html_end - index_html_start;
    return send_response(req, (const char*)index_html_start, index_html_len, "text/html");
}

/**
 * CSS文件处理器
 */
static esp_err_t style_handler(httpd_req_t *req)
{
    const size_t style_css_len = style_css_end - style_css_start;
    return send_response(req, (const char*)style_css_start, style_css_len, "text/css");
}

/**
 * JavaScript文件处理器
 */
static esp_err_t script_handler(httpd_req_t *req)
{
    const size_t script_js_len = script_js_end - script_js_start;
    return send_response(req, (const char*)script_js_start, script_js_len, "application/javascript");
}

/**
 * 图标文件处理器
 */
static esp_err_t favicon_handler(httpd_req_t *req)
{
    const size_t favicon_ico_len = favicon_ico_end - favicon_ico_start;
    return send_response(req, (const char*)favicon_ico_start, favicon_ico_len, "image/x-icon");
}

/**
 * 系统状态API处理器
 */
static esp_err_t api_status_handler(httpd_req_t *req)
{
    cJSON *json = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    
    // 获取KVM状态
    const kvm_status_t *kvm_status = kvm_controller_get_status();
    cJSON_AddNumberToObject(data, "current_channel", kvm_status->current_channel);
    
    // 获取WiFi状态
    const wifi_status_t *wifi_status = wifi_manager_get_status();
    cJSON *wifi_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi_obj, "connected", wifi_status->sta_connected);
    cJSON_AddStringToObject(wifi_obj, "ssid", wifi_status->sta_ssid);
    cJSON_AddStringToObject(wifi_obj, "ip", wifi_status->sta_ip);
    cJSON_AddNumberToObject(wifi_obj, "rssi", wifi_status->sta_rssi);
    cJSON_AddItemToObject(data, "wifi_status", wifi_obj);
    
    // 获取通信状态
    const uart_comm_status_t *comm_status = uart_comm_get_status();
    cJSON *comm_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(comm_obj, "connected", comm_status->connected);
    cJSON_AddNumberToObject(comm_obj, "tx_count", comm_status->tx_count);
    cJSON_AddNumberToObject(comm_obj, "rx_count", comm_status->rx_count);
    cJSON_AddNumberToObject(comm_obj, "error_count", comm_status->error_count);
    cJSON_AddItemToObject(data, "comm_status", comm_obj);
    
    // 获取IP地址
    char ip_str[16];
    if (wifi_manager_get_ip(ip_str, sizeof(ip_str)) == ESP_OK) {
        cJSON_AddStringToObject(data, "ip_address", ip_str);
    }
    
    // 获取运行时间
    uint32_t uptime = esp_timer_get_time() / 1000000; // 转换为秒
    cJSON_AddNumberToObject(data, "uptime", uptime);
    
    // 获取统计信息
    cJSON *stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "total_switches", kvm_status->total_switches);
    cJSON_AddNumberToObject(stats, "error_count", kvm_status->error_count);
    if (kvm_status->total_switches > 0) {
        // 计算最后切换时间（这里简化处理）
        cJSON_AddNumberToObject(stats, "last_switch_time", esp_timer_get_time() / 1000000);
    }
    cJSON_AddItemToObject(data, "stats", stats);
    
    // 获取通道信息
    cJSON *channels = cJSON_CreateArray();
    for (int i = 1; i <= KVM_CHANNEL_MAX; i++) {
        const kvm_channel_info_t *channel_info = kvm_controller_get_channel_info(i);
        if (channel_info) {
            cJSON *channel = cJSON_CreateObject();
            cJSON_AddNumberToObject(channel, "channel", channel_info->channel);
            cJSON_AddBoolToObject(channel, "active", channel_info->active);
            cJSON_AddBoolToObject(channel, "connected", channel_info->connected);
            cJSON_AddStringToObject(channel, "name", channel_info->name);
            cJSON_AddItemToArray(channels, channel);
        }
    }
    cJSON_AddItemToObject(data, "channels", channels);
    
    // 构建响应
    cJSON_AddNumberToObject(json, "code", 0);
    cJSON_AddStringToObject(json, "message", "success");
    cJSON_AddItemToObject(json, "data", data);
    
    char *json_string = cJSON_Print(json);
    esp_err_t ret = send_response(req, json_string, strlen(json_string), "application/json");
    
    free(json_string);
    cJSON_Delete(json);
    
    return ret;
}

/**
 * 通道切换API处理器 (简化版)
 * 调用切换后立即返回成功
 */
static esp_err_t api_switch_handler(httpd_req_t *req)
{
    // 删除调试信息，按用户要求简化日志

    int channel = -1; // 初始化为无效值

    // 从URL路径解析通道号 (例如 /api/switch/2)
    const char *uri = req->uri;
    const char *channel_pos = strrchr(uri, '/');
    if (channel_pos && strlen(channel_pos) > 1) {
        channel = atoi(channel_pos + 1);
    } else {
        // 从POST数据解析
        char content[100];
        int content_len = httpd_req_recv(req, content, sizeof(content) - 1);
        if (content_len > 0) {
            content[content_len] = '\0';
            cJSON *json_body = cJSON_Parse(content);
            if (json_body) {
                cJSON *channel_json = cJSON_GetObjectItem(json_body, "channel");
                if (cJSON_IsNumber(channel_json)) {
                    channel = channel_json->valueint;
                }
                cJSON_Delete(json_body);
            }
        }

        // 如果POST中没有，则从查询参数解析 (例如 /api/switch?channel=2)
        if (channel == -1) {
            char query[64];
            if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
                char param[16];
                if (httpd_query_key_value(query, "channel", param, sizeof(param)) == ESP_OK) {
                    channel = atoi(param);
                }
            }
        }
    }

    cJSON *json_resp = cJSON_CreateObject();

    if (!kvm_controller_is_valid_channel(channel)) {
        cJSON_AddNumberToObject(json_resp, "code", 1);
        cJSON_AddStringToObject(json_resp, "message", "Invalid or missing channel number");
        ESP_LOGE(TAG, "Invalid channel number provided.");
    } else {
        // 调用控制器进行切换 (此函数现在是异步的)
        esp_err_t switch_result = kvm_controller_switch_channel(channel);

        if (switch_result == ESP_OK) {
            // 立即返回成功响应
            cJSON_AddNumberToObject(json_resp, "code", 0);
            cJSON_AddStringToObject(json_resp, "message", "Switch command sent successfully");
            cJSON_AddNumberToObject(json_resp, "channel", channel);
            // 删除成功日志，按用户要求简化输出

            // WebSocket功能已禁用，删除通知
        } else {
            cJSON_AddNumberToObject(json_resp, "code", 1);
            cJSON_AddStringToObject(json_resp, "message", "Switch failed");
            cJSON_AddNumberToObject(json_resp, "channel", channel);
        }
    }

    char *json_string = cJSON_Print(json_resp);
    esp_err_t result = send_response(req, json_string, strlen(json_string), "application/json");

    free(json_string);
    cJSON_Delete(json_resp);

    return result;
}

/**
 * 通道列表API处理器
 */
static esp_err_t api_channels_handler(httpd_req_t *req)
{
    cJSON *json = cJSON_CreateObject();
    cJSON *channels = cJSON_CreateArray();
    
    for (int i = 1; i <= KVM_CHANNEL_MAX; i++) {
        const kvm_channel_info_t *channel_info = kvm_controller_get_channel_info(i);
        if (channel_info) {
            cJSON *channel = cJSON_CreateObject();
            cJSON_AddNumberToObject(channel, "channel", channel_info->channel);
            cJSON_AddBoolToObject(channel, "active", channel_info->active);
            cJSON_AddBoolToObject(channel, "connected", channel_info->connected);
            cJSON_AddStringToObject(channel, "name", channel_info->name);
            cJSON_AddNumberToObject(channel, "switch_count", channel_info->switch_count);
            cJSON_AddItemToArray(channels, channel);
        }
    }
    
    cJSON_AddNumberToObject(json, "code", 0);
    cJSON_AddStringToObject(json, "message", "success");
    cJSON_AddItemToObject(json, "data", channels);
    
    char *json_string = cJSON_Print(json);
    esp_err_t ret = send_response(req, json_string, strlen(json_string), "application/json");
    
    free(json_string);
    cJSON_Delete(json);
    
    return ret;
}

/**
 * WiFi信息API处理器
 */
static esp_err_t api_wifi_handler(httpd_req_t *req)
{
    cJSON *json = cJSON_CreateObject();
    
    const wifi_status_t *wifi_status = wifi_manager_get_status();
    
    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "sta_connected", wifi_status->sta_connected);
    cJSON_AddBoolToObject(data, "ap_started", wifi_status->ap_started);
    cJSON_AddStringToObject(data, "sta_ssid", wifi_status->sta_ssid);
    cJSON_AddStringToObject(data, "sta_ip", wifi_status->sta_ip);
    cJSON_AddStringToObject(data, "ap_ip", wifi_status->ap_ip);
    cJSON_AddNumberToObject(data, "sta_rssi", wifi_status->sta_rssi);
    cJSON_AddNumberToObject(data, "connected_clients", wifi_status->connected_clients);
    
    cJSON_AddNumberToObject(json, "code", 0);
    cJSON_AddStringToObject(json, "message", "success");
    cJSON_AddItemToObject(json, "data", data);
    
    char *json_string = cJSON_Print(json);
    esp_err_t ret = send_response(req, json_string, strlen(json_string), "application/json");

    free(json_string);
    cJSON_Delete(json);

    return ret;
}

/**
 * 启动Web服务器
 */
esp_err_t web_server_start(void)
{
    if (server != NULL) {
        ESP_LOGW(TAG, "Web服务器已经在运行");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WEB_SERVER_PORT;
    config.max_open_sockets = WEB_SERVER_MAX_CLIENTS;
    config.stack_size = WEB_SERVER_STACK_SIZE;
    config.task_priority = 5;
    config.lru_purge_enable = true;

    if (httpd_start(&server, &config) == ESP_OK) {
        // Web服务器启动成功

        // 注册静态文件处理器
        httpd_uri_t index_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = index_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t style_uri = {
            .uri       = "/style.css",
            .method    = HTTP_GET,
            .handler   = style_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &style_uri);

        httpd_uri_t script_uri = {
            .uri       = "/script.js",
            .method    = HTTP_GET,
            .handler   = script_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &script_uri);

        httpd_uri_t favicon_uri = {
            .uri       = "/favicon.ico",
            .method    = HTTP_GET,
            .handler   = favicon_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &favicon_uri);

        // 注册API处理器
        httpd_uri_t api_status_uri = {
            .uri       = "/api/status",
            .method    = HTTP_GET,
            .handler   = api_status_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &api_status_uri);

        // 注册通道切换API - 支持具体通道号（使用静态数组避免内存泄漏）
        static httpd_uri_t switch_uris[8];
        static char switch_uri_paths[8][32];

        for (int i = 1; i <= 8; i++) {
            snprintf(switch_uri_paths[i-1], sizeof(switch_uri_paths[i-1]), "/api/switch/%d", i);

            switch_uris[i-1].uri = switch_uri_paths[i-1];
            switch_uris[i-1].method = HTTP_POST;
            switch_uris[i-1].handler = api_switch_handler;
            switch_uris[i-1].user_ctx = NULL;

            httpd_register_uri_handler(server, &switch_uris[i-1]);
        }

        // 也注册通用的切换API（用于查询参数方式）
        httpd_uri_t api_switch_general_uri = {
            .uri       = "/api/switch",
            .method    = HTTP_POST,
            .handler   = api_switch_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &api_switch_general_uri);

        // 注册OPTIONS处理器（用于CORS预检）
        httpd_uri_t options_uri = {
            .uri       = "/api/*",
            .method    = HTTP_OPTIONS,
            .handler   = options_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &options_uri);

        httpd_uri_t api_channels_uri = {
            .uri       = "/api/channels",
            .method    = HTTP_GET,
            .handler   = api_channels_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &api_channels_uri);

        httpd_uri_t api_wifi_uri = {
            .uri       = "/api/wifi",
            .method    = HTTP_GET,
            .handler   = api_wifi_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &api_wifi_uri);

        // WebSocket功能已禁用，跳过注册

        // URI处理器注册完成
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Web服务器启动失败");
        return ESP_FAIL;
    }
}

/**
 * 停止Web服务器
 */
esp_err_t web_server_stop(void)
{
    if (server == NULL) {
        ESP_LOGW(TAG, "Web服务器未运行");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止Web服务器");
    esp_err_t ret = httpd_stop(server);
    server = NULL;

    return ret;
}

/**
 * 检查服务器是否运行
 */
bool web_server_is_running(void)
{
    return server != NULL;
}
