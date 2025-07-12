/**
 * Web服务器头文件
 * 功能: HTTP服务器和WebSocket通信
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// 服务器配置
#define WEB_SERVER_PORT         80
#define WEB_SERVER_MAX_CLIENTS  10
#define WEB_SERVER_STACK_SIZE   8192

// API路径定义
#define API_ROOT                "/api"
#define API_STATUS              "/api/status"
#define API_SWITCH              "/api/switch"
#define API_CHANNELS            "/api/channels"
#define API_WIFI                "/api/wifi"
#define API_SCAN                "/api/scan"
#define API_CONFIG              "/api/config"

// WebSocket路径
#define WS_PATH                 "/ws"

// HTTP响应类型
typedef enum {
    HTTP_RESPONSE_JSON,
    HTTP_RESPONSE_HTML,
    HTTP_RESPONSE_CSS,
    HTTP_RESPONSE_JS,
    HTTP_RESPONSE_ICO
} http_response_type_t;

// API响应结构
typedef struct {
    int code;
    char message[128];
    char data[512];
} api_response_t;

/**
 * 启动Web服务器
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t web_server_start(void);

/**
 * 停止Web服务器
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t web_server_stop(void);

/**
 * 检查服务器是否运行
 * @return true 运行中，false 已停止
 */
bool web_server_is_running(void);

/**
 * 广播WebSocket消息
 * @param message 消息内容
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t web_server_broadcast_ws_message(const char *message);

/**
 * 发送API响应
 * @param req HTTP请求对象
 * @param response 响应数据
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t web_server_send_api_response(httpd_req_t *req, const api_response_t *response);

/**
 * 发送文件响应
 * @param req HTTP请求对象
 * @param file_data 文件数据
 * @param file_size 文件大小
 * @param content_type 内容类型
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t web_server_send_file(httpd_req_t *req, const char *file_data, size_t file_size, http_response_type_t type);

/**
 * 解析POST数据
 * @param req HTTP请求对象
 * @param buffer 数据缓冲区
 * @param buffer_size 缓冲区大小
 * @return 实际读取的数据长度
 */
int web_server_parse_post_data(httpd_req_t *req, char *buffer, size_t buffer_size);

/**
 * 向所有WebSocket客户端广播消息
 * @param message 要广播的消息
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t web_server_broadcast_ws_message(const char *message);

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_H
