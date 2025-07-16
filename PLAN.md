# KVM 切换器项目开发计划

## 里程碑 1: 基础设置与串口通信

-   [ ] **任务 1.1**: 初始化 ESP-IDF 项目。在根目录运行 `idf.py create-project app` 创建一个名为 `app` 的项目。
-   [ ] **任务 1.2**: 配置项目。运行 `idf.py menuconfig`，完成以下配置：
    -   配置 UART 参数（引脚 TX/RX, 波特率等）以匹配你的主控板。
    -   配置 Wi-Fi 参数（SSID 和密码），让 ESP32 可以连接到你的局域网。
-   [ ] **任务 1.3**: 编写串口发送模块。
    -   在 `app/main` 中创建一个新的 `uart_control.c` 和 `uart_control.h` 文件。
    -   在 `uart_control.c` 中实现 `uart_init()` 函数，用于初始化 UART。
    -   实现三个核心函数，用于发送固定的21位指令：
        -   `void send_switch_to_channel_1(void);`
        -   `void send_switch_to_channel_2(void);`
        -   `void send_switch_failure(void);` (此函数可能在后续错误处理中使用)
-   [ ] **任务 1.4**: 在 `main.c` 中测试串口。调用 `uart_init()`，然后通过延时循环调用 `send_switch_to_channel_1()` 和 `send_switch_to_channel_2()`，用串口调试工具检查主控板是否收到了正确的21位数据。

## 里程碑 2: Web 服务器与前端界面

-   [ ] **任务 2.1**: 建立 Wi-Fi 连接。在 `main.c` 中编写代码，让 ESP32 在启动后自动连接到 Wi-Fi。
-   [ ] **任务 2.2**: 启动 HTTP 服务器。
    -   在 `main.c` 中添加 `esp_http_server` 组件。
    -   编写代码以启动一个基础的 HTTP 服务器。
-   [ ] **任务 2.3**: 创建 HTML 前端页面。
    -   创建一个简单的 `index.html` 文件。
    -   页面包含两个按钮：“切换到通道 1” 和 “切换到通道 2”。
    -   包含一个用于显示状态的区域（例如：“未连接”，“切换成功”）。
-   [ ] **任务 2.4**: 将前端文件嵌入固件。使用 ESP-IDF 的 `spiffs` 或 `embed` 功能，将 `index.html` 文件打包进 ESP32 的固件中。
-   [ ] **任务 2.5**: 创建根 URL 处理程序。设置一个 HTTP GET `/` 的处理程序，当用户访问 ESP32 的 IP 地址时，返回 `index.html` 的内容。

## 里程碑 3: 前后端集成与联调

-   [ ] **任务 3.1**: 创建 API 接口 (Endpoints)。
    -   创建 HTTP GET `/switch/1` 的处理程序。
    -   创建 HTTP GET `/switch/2` 的处理程序。
-   [ ] **任务 3.2**: 编写前端 JavaScript。
    -   在 `index.html` 中添加 JavaScript 代码。
    -   为两个按钮添加点击事件监听器。
    -   当按钮被点击时，使用 `fetch` API 向对应的 URL (例如 `http://<ESP32_IP>/switch/1`) 发送请求。
    -   根据服务器的响应更新状态显示区域。
-   [ ] **任务 3.3**: 连接后端逻辑。
    -   在 `/switch/1` 的处理程序中，调用 `send_switch_to_channel_1()` 函数。
    -   在 `/switch/2` 的处理程序中，调用 `send_switch_to_channel_2()` 函数。
    -   让处理程序返回一个简单的 JSON 响应，例如 `{"status": "ok", "channel": 1}`。
-   [ ] **任务 3.4**: 完整系统测试。
    -   编译并烧录完整固件。
    -   在浏览器中打开 ESP32 的 IP 地址。
    -   点击按钮，并同时观察主控板的串口是否收到正确的指令，以及网页的状态是否更新。

## 里程碑 4: 优化与健壮性 (可选)

-   [ ] **任务 4.1**: 错误处理。如果主控板可以返回响应，则增加串口接收逻辑，判断切换是否真的成功，并将真实结果返回给前端。
-   [ ] **任务 4.2**: 美化 Web 界面。使用 CSS 美化按钮和布局。
-   [ ] **任务 4.3**: Wi-Fi 管理。添加一个 Wi-Fi 管理页面，允许用户在网页上配置要连接的 Wi-Fi，而不是硬编码在代码里。