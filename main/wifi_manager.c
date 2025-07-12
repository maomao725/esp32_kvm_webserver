/**
 * WiFi管理器实现
 * 功能: 管理WiFi连接，支持STA和AP模式
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "wifi_manager.h"

static const char *TAG = "WIFI_MGR";

// WiFi事件组
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// WiFi状态
static wifi_status_t s_wifi_status = {0};
static int s_retry_num = 0;

// 网络接口
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;

/**
 * WiFi事件处理函数
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        // STA模式启动
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_RETRY_MAX) {
            esp_wifi_connect();
            s_retry_num++;
            // 重试连接WiFi
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "WiFi连接失败，已达到最大重试次数");
        }
        s_wifi_status.sta_connected = false;
        memset(s_wifi_status.sta_ip, 0, sizeof(s_wifi_status.sta_ip));
        
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        snprintf(s_wifi_status.sta_ip, sizeof(s_wifi_status.sta_ip), 
                IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "获得IP地址: %s", s_wifi_status.sta_ip);
        
        s_retry_num = 0;
        s_wifi_status.sta_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        // 客户端连接到AP
        s_wifi_status.connected_clients++;
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        // 客户端断开AP连接
        if (s_wifi_status.connected_clients > 0) {
            s_wifi_status.connected_clients--;
        }
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        // AP模式启动成功
        s_wifi_status.ap_started = true;
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
        // AP模式已停止
        s_wifi_status.ap_started = false;
        s_wifi_status.connected_clients = 0;
    }
}

/**
 * 初始化WiFi管理器
 */
esp_err_t wifi_manager_init(void)
{
    
    // 创建事件组
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "创建WiFi事件组失败");
        return ESP_FAIL;
    }
    
    // 创建网络接口
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    
    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // 注册事件处理器
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    
    // 首先尝试STA模式连接WiFi
    // 简化连接尝试日志
    esp_err_t ret = wifi_manager_connect_sta(DEFAULT_STA_SSID, DEFAULT_STA_PASSWORD);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "STA连接失败，启动AP模式");
        // STA连接失败，启动AP模式
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ret = wifi_manager_start_ap(DEFAULT_AP_SSID, DEFAULT_AP_PASSWORD);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "启动AP模式失败");
            return ret;
        }
        strcpy(s_wifi_status.ap_ip, "192.168.4.1");
    } else {
        // STA模式连接成功
    }

    // WiFi管理器初始化完成
    return ESP_OK;
}

/**
 * 启动AP模式
 */
esp_err_t wifi_manager_start_ap(const char *ssid, const char *password)
{
    if (ssid == NULL) {
        ESP_LOGE(TAG, "AP SSID不能为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    wifi_config_t wifi_config = {
        .ap = {
            .channel = DEFAULT_AP_CHANNEL,
            .max_connection = DEFAULT_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    
    // 设置SSID
    strncpy((char*)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(ssid);
    
    // 设置密码
    if (password != NULL && strlen(password) > 0) {
        strncpy((char*)wifi_config.ap.password, password, sizeof(wifi_config.ap.password) - 1);
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // AP启动成功
    return ESP_OK;
}

/**
 * 连接到WiFi网络
 */
esp_err_t wifi_manager_connect_sta(const char *ssid, const char *password)
{
    if (ssid == NULL) {
        ESP_LOGE(TAG, "WiFi SSID不能为空");
        return ESP_ERR_INVALID_ARG;
    }

    // 首先设置WiFi模式为STA模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config = {0};

    // 设置SSID
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);

    // 设置密码
    if (password != NULL) {
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // 启动WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    // 保存SSID到状态
    strncpy(s_wifi_status.sta_ssid, ssid, sizeof(s_wifi_status.sta_ssid) - 1);

    // 开始连接WiFi
    
    // 等待连接结果（最多等待10秒）
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(10000));
    
    if (bits & WIFI_CONNECTED_BIT) {
        // WiFi连接成功
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "WiFi连接失败");
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "WiFi连接超时");
        return ESP_ERR_TIMEOUT;
    }
}

/**
 * 断开WiFi连接
 */
esp_err_t wifi_manager_disconnect(void)
{
    // 断开WiFi连接
    return esp_wifi_disconnect();
}

/**
 * 获取WiFi状态
 */
const wifi_status_t* wifi_manager_get_status(void)
{
    // 更新RSSI
    if (s_wifi_status.sta_connected) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            s_wifi_status.sta_rssi = ap_info.rssi;
        }
    }
    
    return &s_wifi_status;
}

/**
 * 检查是否已连接WiFi
 */
bool wifi_manager_is_connected(void)
{
    return s_wifi_status.sta_connected;
}

/**
 * 获取本机IP地址
 */
esp_err_t wifi_manager_get_ip(char *ip_str, size_t len)
{
    if (ip_str == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_wifi_status.sta_connected) {
        strncpy(ip_str, s_wifi_status.sta_ip, len - 1);
        ip_str[len - 1] = '\0';
        return ESP_OK;
    } else if (s_wifi_status.ap_started) {
        strncpy(ip_str, s_wifi_status.ap_ip, len - 1);
        ip_str[len - 1] = '\0';
        return ESP_OK;
    }
    
    return ESP_FAIL;
}

/**
 * 扫描可用WiFi网络
 */
uint16_t wifi_manager_scan_networks(wifi_ap_record_t *scan_result, uint16_t max_records)
{
    if (scan_result == NULL || max_records == 0) {
        return 0;
    }
    
    // 开始扫描WiFi网络
    
    // 启动扫描
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false
    };
    
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi扫描启动失败: %s", esp_err_to_name(ret));
        return 0;
    }
    
    // 获取扫描结果
    uint16_t number = max_records;
    ret = esp_wifi_scan_get_ap_records(&number, scan_result);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取WiFi扫描结果失败: %s", esp_err_to_name(ret));
        return 0;
    }
    
    // 扫描完成
    return number;
}
