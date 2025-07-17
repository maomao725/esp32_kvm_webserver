/**
 * ESP32-S3 KVM Web服务器主程序
 * 功能: 提供网页控制界面，管理HDMI通道切换
 * 作者: 叶家乐
 * 日期: 2025-06-28
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "cJSON.h"

#include "wifi_manager.h"
#include "web_server.h"
#include "kvm_controller.h"
#include "uart_comm.h"
#include "driver/uart.h"

static const char *TAG = "KVM_MAIN";

// 自定义printf函数，重定向到UART1 (GPIO17/18)
static int uart1_vprintf(const char *format, va_list args) {
    char buffer[512];
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    if (len > 0) {
        uart_write_bytes(UART_NUM_1, buffer, len);
    }
    return len;
}

// 系统状态LED
#define STATUS_LED_GPIO     GPIO_NUM_2
#define LED_ON              1
#define LED_OFF             0

/**
 * 初始化状态LED
 */
static void init_status_led(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << STATUS_LED_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    gpio_set_level(STATUS_LED_GPIO, LED_OFF);
}

/**
 * 状态LED闪烁任务
 */
static void status_led_task(void *pvParameters)
{
    bool led_state = false;
    
    while (1) {
        led_state = !led_state;
        gpio_set_level(STATUS_LED_GPIO, led_state ? LED_ON : LED_OFF);
        
        // 根据系统状态调整闪烁频率
        if (wifi_manager_is_connected()) {
            vTaskDelay(pdMS_TO_TICKS(2000)); // 慢闪：已连接WiFi
        } else {
            vTaskDelay(pdMS_TO_TICKS(500));  // 快闪：未连接WiFi
        }
    }
}

/**
 * 系统监控任务
 */
static void system_monitor_task(void *pvParameters)
{
    while (1) {
        // 只保留内存监控，删除其他调试信息
        if (esp_get_free_heap_size() < 50000) {
            ESP_LOGW(TAG, "警告: 可用内存不足!");
        }

        vTaskDelay(pdMS_TO_TICKS(30000)); // 每30秒监控一次
    }
}

/**
 * WebSocket状态推送任务
 */
static void websocket_status_task(void *pvParameters)
{
    while (1) {
        // 构建状态更新消息
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

        // 构建WebSocket消息
        cJSON_AddStringToObject(json, "type", "status_update");
        cJSON_AddItemToObject(json, "data", data);

        char *json_string = cJSON_Print(json);
        if (json_string) {
            web_server_broadcast_ws_message(json_string);
            free(json_string);
        }

        cJSON_Delete(json);

        vTaskDelay(pdMS_TO_TICKS(5000)); // 每5秒推送一次状态
    }
}

/**
 * 应用程序主函数
 */
void app_main(void)
{
    // 简化启动信息
    
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 初始化状态LED
    init_status_led();
    
    // 配置调试日志输出到GPIO17/18 (UART1)
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // 配置UART1用于调试日志输出
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, GPIO_NUM_17, GPIO_NUM_18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 1024, 1024, 0, NULL, 0));
    
    // 重定向console输出到UART1
    esp_log_set_vprintf(uart1_vprintf);

    // 初始化UART通信
    ESP_ERROR_CHECK(uart_comm_init());

    // 初始化KVM控制器
    kvm_controller_init();

    // 初始化WiFi管理器
    wifi_manager_init();

    // 启动Web服务器
    esp_err_t web_ret = web_server_start();
    if (web_ret != ESP_OK) {
        ESP_LOGE(TAG, "Web服务器启动失败: %s", esp_err_to_name(web_ret));
    }
    
    // 创建状态LED任务
    xTaskCreate(status_led_task, "status_led", 2048, NULL, 5, NULL);

    // 创建系统监控任务
    xTaskCreate(system_monitor_task, "sys_monitor", 4096, NULL, 3, NULL);

    // WebSocket功能已禁用，不创建状态推送任务
    // xTaskCreate(websocket_status_task, "ws_status", 4096, NULL, 4, NULL);
    
    // 主循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
