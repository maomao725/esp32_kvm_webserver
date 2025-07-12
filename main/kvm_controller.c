/**
 * KVM控制器实现
 * 功能: 管理HDMI通道切换和状态
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"

#include "kvm_controller.h"
#include "uart_comm.h"

static const char *TAG = "KVM_CTRL";

// KVM系统状态
static kvm_status_t s_kvm_status = {0};
static SemaphoreHandle_t s_kvm_mutex = NULL;

// 默认通道名称
static const char* default_channel_names[KVM_CHANNEL_MAX] = {
    "电脑1", "电脑2"
};

/**
 * 初始化KVM控制器
 */
esp_err_t kvm_controller_init(void)
{
    // 简化初始化日志
    
    // 创建互斥锁
    s_kvm_mutex = xSemaphoreCreateMutex();
    if (s_kvm_mutex == NULL) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_FAIL;
    }
    
    // 初始化状态
    memset(&s_kvm_status, 0, sizeof(s_kvm_status));
    s_kvm_status.current_channel = KVM_CHANNEL_DEFAULT;
    s_kvm_status.target_channel = KVM_CHANNEL_DEFAULT;
    s_kvm_status.switch_status = KVM_SWITCH_IDLE;
    s_kvm_status.communication_ok = false;
    
    // 初始化通道信息
    for (int i = 0; i < KVM_CHANNEL_MAX; i++) {
        s_kvm_status.channels[i].channel = i + 1;
        s_kvm_status.channels[i].active = (i + 1 == KVM_CHANNEL_DEFAULT);
        s_kvm_status.channels[i].connected = true; // 假设所有通道都已连接
        strncpy(s_kvm_status.channels[i].name, default_channel_names[i], 
                sizeof(s_kvm_status.channels[i].name) - 1);
        s_kvm_status.channels[i].switch_count = 0;
        s_kvm_status.channels[i].last_switch_time = 0;
    }
    
    // 简化初始化完成日志
    return ESP_OK;
}

/**
 * 切换到指定通道 (简化版)
 * 发送指令后立即更新状态，不等待响应
 */
esp_err_t kvm_controller_switch_channel(int channel)
{
    if (!kvm_controller_is_valid_channel(channel)) {
        ESP_LOGE(TAG, "Invalid channel number: %d", channel);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_kvm_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire KVM mutex");
        return ESP_ERR_TIMEOUT;
    }

    // 删除切换尝试的调试信息

    // 如果已经是目标通道，则不执行任何操作
    if (s_kvm_status.current_channel == channel) {
        // 已经是目标通道
        xSemaphoreGive(s_kvm_mutex);
        return ESP_OK;
    }

    // 设置目标通道和状态
    s_kvm_status.target_channel = channel;
    s_kvm_status.switch_status = KVM_SWITCH_IN_PROGRESS;

    // 通过UART发送切换命令
    esp_err_t ret = uart_comm_switch_channel(channel);

    if (ret != ESP_OK) {
        // 如果UART发送失败，记录错误并返回
        s_kvm_status.switch_status = KVM_SWITCH_FAILED;
        s_kvm_status.error_count++;
        s_kvm_status.communication_ok = false;
        ESP_LOGE(TAG, "Failed to send switch command to UART, error: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_kvm_mutex);
        return ret;
    }

    // 立即更新状态，不等待CH32V003响应
    // 更新旧通道状态
    if (s_kvm_status.current_channel >= 1 && s_kvm_status.current_channel <= KVM_CHANNEL_MAX) {
        s_kvm_status.channels[s_kvm_status.current_channel - 1].active = false;
    }

    // 更新新通道状态
    s_kvm_status.current_channel = channel;
    s_kvm_status.channels[channel - 1].active = true;
    s_kvm_status.channels[channel - 1].switch_count++;
    s_kvm_status.channels[channel - 1].last_switch_time = esp_timer_get_time() / 1000000;

    // 更新系统统计
    s_kvm_status.total_switches++;
    s_kvm_status.switch_status = KVM_SWITCH_SUCCESS;
    s_kvm_status.communication_ok = true;

    // 删除切换成功的调试信息

    xSemaphoreGive(s_kvm_mutex);
    return ret; // 总是返回成功
}

/**
 * 获取当前活跃通道
 */
int kvm_controller_get_current_channel(void)
{
    return s_kvm_status.current_channel;
}

/**
 * 获取KVM系统状态
 */
const kvm_status_t* kvm_controller_get_status(void)
{
    return &s_kvm_status;
}

/**
 * 检查通道是否有效
 */
bool kvm_controller_is_valid_channel(int channel)
{
    return (channel >= KVM_CHANNEL_MIN && channel <= KVM_CHANNEL_MAX);
}

/**
 * 设置通道名称
 */
esp_err_t kvm_controller_set_channel_name(int channel, const char *name)
{
    if (!kvm_controller_is_valid_channel(channel) || name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_kvm_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    strncpy(s_kvm_status.channels[channel - 1].name, name, 
            sizeof(s_kvm_status.channels[channel - 1].name) - 1);
    s_kvm_status.channels[channel - 1].name[sizeof(s_kvm_status.channels[channel - 1].name) - 1] = '\0';
    
    xSemaphoreGive(s_kvm_mutex);
    
    // 通道名称已更新
    return ESP_OK;
}

/**
 * 获取通道信息
 */
const kvm_channel_info_t* kvm_controller_get_channel_info(int channel)
{
    if (!kvm_controller_is_valid_channel(channel)) {
        return NULL;
    }
    
    return &s_kvm_status.channels[channel - 1];
}

/**
 * 检测通道连接状态
 */
bool kvm_controller_is_channel_connected(int channel)
{
    if (!kvm_controller_is_valid_channel(channel)) {
        return false;
    }
    
    return s_kvm_status.channels[channel - 1].connected;
}

/**
 * 获取切换状态
 */
kvm_switch_status_t kvm_controller_get_switch_status(void)
{
    return s_kvm_status.switch_status;
}

/**
 * 重置错误计数
 */
void kvm_controller_reset_error_count(void)
{
    if (xSemaphoreTake(s_kvm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        s_kvm_status.error_count = 0;
        xSemaphoreGive(s_kvm_mutex);
        // 错误计数已重置
    }
}

/**
 * 获取统计信息JSON字符串
 */
esp_err_t kvm_controller_get_stats_json(char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    cJSON *json = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(json, "current_channel", s_kvm_status.current_channel);
    cJSON_AddNumberToObject(json, "total_switches", s_kvm_status.total_switches);
    cJSON_AddNumberToObject(json, "error_count", s_kvm_status.error_count);
    cJSON_AddBoolToObject(json, "communication_ok", s_kvm_status.communication_ok);
    
    // 计算成功率
    float success_rate = 100.0f;
    if (s_kvm_status.total_switches > 0) {
        success_rate = ((float)(s_kvm_status.total_switches - s_kvm_status.error_count) / 
                       s_kvm_status.total_switches) * 100.0f;
    }
    cJSON_AddNumberToObject(json, "success_rate", success_rate);
    
    // 添加通道信息
    cJSON *channels = cJSON_CreateArray();
    for (int i = 0; i < KVM_CHANNEL_MAX; i++) {
        cJSON *channel = cJSON_CreateObject();
        cJSON_AddNumberToObject(channel, "channel", s_kvm_status.channels[i].channel);
        cJSON_AddBoolToObject(channel, "active", s_kvm_status.channels[i].active);
        cJSON_AddBoolToObject(channel, "connected", s_kvm_status.channels[i].connected);
        cJSON_AddStringToObject(channel, "name", s_kvm_status.channels[i].name);
        cJSON_AddNumberToObject(channel, "switch_count", s_kvm_status.channels[i].switch_count);
        cJSON_AddNumberToObject(channel, "last_switch_time", s_kvm_status.channels[i].last_switch_time);
        cJSON_AddItemToArray(channels, channel);
    }
    cJSON_AddItemToObject(json, "channels", channels);
    
    char *json_string = cJSON_Print(json);
    if (json_string == NULL) {
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    size_t json_len = strlen(json_string);
    if (json_len >= buffer_size) {
        free(json_string);
        cJSON_Delete(json);
        return ESP_ERR_NO_MEM;
    }
    
    strcpy(buffer, json_string);
    
    free(json_string);
    cJSON_Delete(json);
    
    return ESP_OK;
}
