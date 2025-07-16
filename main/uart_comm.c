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
    ESP_LOGI(TAG, "开始初始化UART通信...");

    // 创建互斥锁
    uart_mutex = xSemaphoreCreateMutex();
    if (uart_mutex == NULL) {
        ESP_LOGE(TAG, "创建UART互斥锁失败");
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

    ESP_LOGI(TAG, "UART配置: 端口=%d, 波特率=%d, TX引脚=%d, RX引脚=%d", 
             UART_PORT_NUM, UART_BAUD_RATE, UART_TX_PIN, UART_RX_PIN);

    // 先配置UART参数
    esp_err_t ret = uart_param_config(UART_PORT_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "配置UART参数失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 设置UART引脚
    ret = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN,
                      UART_RTS_PIN, UART_CTS_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置UART引脚失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 安装UART驱动
    ret = uart_driver_install(UART_PORT_NUM, UART_RX_BUFFER_SIZE,
                             UART_TX_BUFFER_SIZE, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "安装UART驱动失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 清空缓冲区
    uart_flush(UART_PORT_NUM);

    ESP_LOGI(TAG, "✓ UART通信初始化完成");
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
        ESP_LOGE(TAG, "无效通道号: %d", channel);
        return ESP_ERR_INVALID_ARG;
    }

    // 定义切换成功响应格式 (根据协议文档)
    // 格式: [起始字节][响应状态][数据长度][当前通道][数据填充15字节][校验和][结束字节]
    uint8_t response_data[21] = {
        0xBB,                                                           // 索引0: 起始字节
        0x00,                                                           // 索引1: 响应状态 (成功)
        0x01,                                                           // 索引2: 数据长度 (1字节)
        (uint8_t)channel,                                               // 索引3: 当前通道号
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,               // 索引4-11: 数据填充 (8字节)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                     // 索引12-18: 数据填充 (7字节)
        0x00,                                                           // 索引19: 校验和 (待计算)
        0x66                                                            // 索引20: 结束字节
    };
    
    // 根据协议图片计算校验和
    // 通道1: BB 00 01 01 00*15 -> 校验和 0xBA
    // 通道2: BB 00 01 02 00*15 -> 校验和 0xB9
    uint8_t checksum;
    if (channel == 1) {
        checksum = 0xBA;  // 协议图片中指定的校验和
    } else if (channel == 2) {
        checksum = 0xB9;  // 协议图片中指定的校验和
    } else {
        // 对于其他通道，使用简单累加算法
        checksum = 0;
        for (int i = 0; i < 20; i++) {
            checksum += response_data[i];
        }
    }
    response_data[19] = checksum;

    const uint8_t *command_to_send = response_data;
    const int command_size = 21; // 明确指定21字节

    ESP_LOGI(TAG, "准备发送通道%d切换响应，数据长度: %d字节", channel, command_size);

    // 打印要发送的数据（用于调试）
    ESP_LOG_BUFFER_HEX(TAG, command_to_send, command_size);
    
    // 验证数据格式的详细输出
    printf("预期hex数据: ");
    for(int i = 0; i < command_size; i++) {
        printf("%02X ", command_to_send[i]);
    }
    printf("\n");
    
    // 打印人类可读的数据格式
    ESP_LOGI(TAG, "数据格式解析:");
    ESP_LOGI(TAG, "  起始字节: 0x%02X", command_to_send[0]);
    ESP_LOGI(TAG, "  响应状态: 0x%02X", command_to_send[1]);
    ESP_LOGI(TAG, "  数据长度: 0x%02X", command_to_send[2]);
    ESP_LOGI(TAG, "  当前通道: 0x%02X (%d)", command_to_send[3], command_to_send[3]);
    ESP_LOGI(TAG, "  校验和: 0x%02X", command_to_send[19]);
    ESP_LOGI(TAG, "  结束字节: 0x%02X", command_to_send[20]);
    ESP_LOGI(TAG, "数据应该在串口助手中显示为二进制数据，这是正常的！");
    
    // 如果需要测试，可以发送一个简单的文本消息
    // 取消注释下面的代码来发送文本而不是二进制数据
    /*
    char test_message[64];
    snprintf(test_message, sizeof(test_message), "KVM_SWITCH_CH%d_OK\r\n", channel);
    ESP_LOGI(TAG, "发送测试文本: %s", test_message);
    uart_write_bytes(UART_PORT_NUM, test_message, strlen(test_message));
    return ESP_OK;
    */

    if (uart_mutex == NULL) {
        ESP_LOGE(TAG, "UART互斥锁未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "获取UART互斥锁失败");
        return ESP_ERR_TIMEOUT;
    }

    // 清空发送和接收缓冲区
    uart_flush(UART_PORT_NUM);
    
    // 等待UART准备就绪
    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(100));

    // 发送指令
    int bytes_sent = uart_write_bytes(UART_PORT_NUM, (const char *)command_to_send, command_size);
    
    // 确保数据发送完成
    esp_err_t wait_result = uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(1000));
    if (wait_result != ESP_OK) {
        ESP_LOGW(TAG, "UART发送等待超时: %s", esp_err_to_name(wait_result));
    }

    xSemaphoreGive(uart_mutex);

    if (bytes_sent == command_size) {
        ESP_LOGI(TAG, "✓ UART成功发送通道%d切换响应 (%d字节)", channel, bytes_sent);
        // 额外验证：再次打印发送的校验和
        ESP_LOGI(TAG, "发送的校验和: 0x%02X", command_to_send[19]);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "✗ UART发送失败 通道%d: 实际发送%d/%d字节", channel, bytes_sent, command_size);
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
    // 无操作
}
