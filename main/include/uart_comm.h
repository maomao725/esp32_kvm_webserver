/**
 * UART通信头文件 
 * 功能: 与CH32V003通信协议实现
 */

#ifndef UART_COMM_H
#define UART_COMM_H

#include "esp_err.h"
#include <stdbool.h>
#include "driver/gpio.h" // 添加缺失的头文件
#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

// UART配置参数
#define UART_PORT_NUM           UART_NUM_1
#define UART_BAUD_RATE          9600
#define UART_DATA_BITS          UART_DATA_8_BITS
#define UART_PARITY             UART_PARITY_DISABLE
#define UART_STOP_BITS          UART_STOP_BITS_1
#define UART_FLOW_CTRL          UART_HW_FLOWCTRL_DISABLE

// GPIO引脚定义 (ESP32-S3适配)
#define UART_TX_PIN             GPIO_NUM_17  // UART1 TX
#define UART_RX_PIN             GPIO_NUM_18  // UART1 RX  
#define UART_RTS_PIN            UART_PIN_NO_CHANGE
#define UART_CTS_PIN            UART_PIN_NO_CHANGE

// 缓冲区大小 (可以适当减小)
#define UART_TX_BUFFER_SIZE     256
#define UART_RX_BUFFER_SIZE     256

// 通信状态 (简化)
typedef struct {
    bool connected;
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t error_count;
    uint64_t last_response_time; // 使用64位以防溢出
} uart_comm_status_t;

/**
 * 初始化UART通信
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t uart_comm_init(void);

/**
 * 发送通道切换命令
 * @param channel 目标通道 (1 或 2)
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t uart_comm_switch_channel(int channel);

/**
 * 检查通信连接状态 (存根)
 * @return true 已连接，false 未连接
 */
bool uart_comm_is_connected(void);

/**
 * 获取通信状态 (存根)
 * @return 通信状态结构体指针
 */
const uart_comm_status_t* uart_comm_get_status(void);

/**
 * 重置通信状态 (存根)
 */
void uart_comm_reset_status(void);

#ifdef __cplusplus
}
#endif

#endif // UART_COMM_H