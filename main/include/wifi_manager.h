/**
 * WiFi管理器头文件
 * 功能: 管理WiFi连接，支持STA和AP模式
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

// WiFi配置参数
#define WIFI_SSID_MAX_LEN       32
#define WIFI_PASSWORD_MAX_LEN   64
#define WIFI_RETRY_MAX          5

// 默认AP配置
#define DEFAULT_AP_SSID         "ESP32-KVM"
#define DEFAULT_AP_PASSWORD     "12345678"
#define DEFAULT_AP_CHANNEL      1
#define DEFAULT_AP_MAX_CONN     4

// 默认STA配置 - 请修改为您的WiFi信息
#define DEFAULT_STA_SSID        "maomao"     // WiFi名称
#define DEFAULT_STA_PASSWORD    "y20050725" // WiFi密码

// WiFi状态
typedef struct {
    bool sta_connected;
    bool ap_started;
    char sta_ssid[WIFI_SSID_MAX_LEN];
    char sta_ip[16];
    char ap_ip[16];
    int sta_rssi;
    int connected_clients;
} wifi_status_t;

/**
 * 初始化WiFi管理器
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t wifi_manager_init(void);

/**
 * 启动AP模式
 * @param ssid AP名称
 * @param password AP密码
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t wifi_manager_start_ap(const char *ssid, const char *password);

/**
 * 连接到WiFi网络
 * @param ssid 网络名称
 * @param password 网络密码
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t wifi_manager_connect_sta(const char *ssid, const char *password);

/**
 * 断开WiFi连接
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t wifi_manager_disconnect(void);

/**
 * 获取WiFi状态
 * @return WiFi状态结构体指针
 */
const wifi_status_t* wifi_manager_get_status(void);

/**
 * 检查是否已连接WiFi
 * @return true 已连接，false 未连接
 */
bool wifi_manager_is_connected(void);

/**
 * 获取本机IP地址
 * @param ip_str IP地址字符串缓冲区
 * @param len 缓冲区长度
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t wifi_manager_get_ip(char *ip_str, size_t len);

/**
 * 扫描可用WiFi网络
 * @param scan_result 扫描结果缓冲区
 * @param max_records 最大记录数
 * @return 实际扫描到的网络数量
 */
uint16_t wifi_manager_scan_networks(wifi_ap_record_t *scan_result, uint16_t max_records);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
