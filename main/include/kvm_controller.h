/**
 * KVM控制器头文件
 * 功能: 管理HDMI通道切换和状态
 */

#ifndef KVM_CONTROLLER_H
#define KVM_CONTROLLER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 通道配置
#define KVM_CHANNEL_MIN         1
#define KVM_CHANNEL_MAX         2
#define KVM_CHANNEL_DEFAULT     1

// 切换状态
typedef enum {
    KVM_SWITCH_IDLE,
    KVM_SWITCH_IN_PROGRESS,
    KVM_SWITCH_SUCCESS,
    KVM_SWITCH_FAILED
} kvm_switch_status_t;

// 通道状态
typedef struct {
    int channel;
    bool active;
    bool connected;
    char name[32];
    uint32_t switch_count;
    uint32_t last_switch_time;
} kvm_channel_info_t;

// KVM系统状态
typedef struct {
    int current_channel;
    int target_channel;
    kvm_switch_status_t switch_status;
    bool communication_ok;
    uint32_t total_switches;
    uint32_t error_count;
    kvm_channel_info_t channels[KVM_CHANNEL_MAX];
} kvm_status_t;

/**
 * 初始化KVM控制器
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t kvm_controller_init(void);

/**
 * 切换到指定通道
 * @param channel 目标通道 (1-2)
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t kvm_controller_switch_channel(int channel);

/**
 * 获取当前活跃通道
 * @return 当前通道号
 */
int kvm_controller_get_current_channel(void);

/**
 * 获取KVM系统状态
 * @return KVM状态结构体指针
 */
const kvm_status_t* kvm_controller_get_status(void);

/**
 * 检查通道是否有效
 * @param channel 通道号
 * @return true 有效，false 无效
 */
bool kvm_controller_is_valid_channel(int channel);

/**
 * 设置通道名称
 * @param channel 通道号
 * @param name 通道名称
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t kvm_controller_set_channel_name(int channel, const char *name);

/**
 * 获取通道信息
 * @param channel 通道号
 * @return 通道信息结构体指针，失败返回NULL
 */
const kvm_channel_info_t* kvm_controller_get_channel_info(int channel);

/**
 * 检测通道连接状态
 * @param channel 通道号
 * @return true 已连接，false 未连接
 */
bool kvm_controller_is_channel_connected(int channel);

/**
 * 获取切换状态
 * @return 切换状态枚举值
 */
kvm_switch_status_t kvm_controller_get_switch_status(void);

/**
 * 重置错误计数
 */
void kvm_controller_reset_error_count(void);

/**
 * 获取统计信息JSON字符串
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t kvm_controller_get_stats_json(char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // KVM_CONTROLLER_H
