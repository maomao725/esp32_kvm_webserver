/**
 * UART通信实现
 * 功能: 向CH32V003发送固定的KVM切换指令
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "uart_comm.h"

static const char *TAG = "UART_COMM";

// UART互斥锁
static SemaphoreHandle_t uart_mutex = NULL;

/**
 * 初始化UART通信
 */
esp_err_t uart_comm_init(void)
{
    // UART通信初始化

    // 创建互斥锁
    uart_mutex = xSemaphoreCreateMutex();
    if (uart_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create UART mutex");
        return ESP_FAIL;
    }

    // 配置UART参数
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_BITS,
        .parity = UART_PARITY,
        .stop_bits = UART_STOP_BITS,
        .flow_ctrl = UART_FLOW_CTRL,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // 安装UART驱动
    esp_err_t ret = uart_driver_install(UART_PORT_NUM, UART_RX_BUFFER_SIZE,
                                       UART_TX_BUFFER_SIZE, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置UART参数
    ret = uart_param_config(UART_PORT_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART parameters: %s", esp_err_to_name(ret));
        return ret;
    }

    // 设置UART引脚
    ret = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN,
                      UART_RTS_PIN, UART_CTS_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
        return ret;
    }

    // UART通信初始化完成，无需调试信息

    return ESP_OK;
}

// 删除uart_comm_read_response函数，按用户要求不等待响应

/**
 * 发送通道切换命令 (简化版)
 * 直接发送21字节的固定指令，不等待响应
 */
esp_err_t uart_comm_switch_channel(int channel)
{
    if (channel < 1 || channel > 2) {
        return ESP_ERR_INVALID_ARG;
    }

    // 定义固定的切换指令
    const uint8_t cmd_ch1[] = {0xBB, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xBA, 0x66};
    const uint8_t cmd_ch2[] = {0xBB, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB9, 0x66};

    const uint8_t *command_to_send = (channel == 1) ? cmd_ch1 : cmd_ch2;
    const int command_size = sizeof(cmd_ch1);

    if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire UART mutex");
        return ESP_ERR_TIMEOUT;
    }

    // 清空接收缓冲区 (以防有干扰数据)
    uart_flush(UART_PORT_NUM);

    // 发送指令
    int bytes_sent = uart_write_bytes(UART_PORT_NUM, (const char *)command_to_send, command_size);

    xSemaphoreGive(uart_mutex);

    if (bytes_sent == command_size) {
        ESP_LOGI(TAG, "UART发送通道%d切换命令 (21字节)", channel);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "UART发送失败 通道%d: 发送%d/%d字节", channel, bytes_sent, command_size);
        return ESP_FAIL;
    }
}

// --- 存根函数，以保持与其他模块的兼容性 ---

/**
 * 获取通信状态
 * 返回一个静态的、默认的“已连接”状态
 */
const uart_comm_status_t* uart_comm_get_status(void)
{
    static const uart_comm_status_t dummy_status = {
        .connected = true,
        .tx_count = 0, // 在这个简化版本中不跟踪计数
        .rx_count = 0,
        .error_count = 0,
        .last_response_time = 0
    };
    return &dummy_status;
}

/**
 * 检查通信连接状态
 * 总是返回true
 */
bool uart_comm_is_connected(void)
{
    return true;
}

/**
 * 重置通信状态
 * 无操作
 */
void uart_comm_reset_status(void)
{
    ;// 无操作
}
