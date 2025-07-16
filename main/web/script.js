/**
 * ESP32-S3 KVM控制器前端JavaScript
 * 功能: 网页交互逻辑和API通信
 */

// 全局变量
let currentChannel = 1;
let isConnected = false;
let websocket = null;
let statusUpdateInterval = null;
let logEntries = [];

// API端点
const API = {
    STATUS: '/api/status',
    SWITCH: '/api/switch',
    CHANNELS: '/api/channels',
    WIFI: '/api/wifi'
};

/**
 * 页面加载完成后初始化
 */
document.addEventListener('DOMContentLoaded', function() {
    console.log('KVM控制器前端初始化...');
    
    // 初始化连接状态 (HTTP模式)
    initWebSocket();
    
    // 开始状态更新
    startStatusUpdate();
    
    // 启动轮询模式
    startPollingMode();
    
    // 初始化界面
    updateUI();
    
    // 添加键盘快捷键
    addKeyboardShortcuts();
    
    addLog('系统', '前端界面初始化完成');
});

/**
 * WebSocket功能已禁用，使用HTTP轮询模式
 */
function initWebSocket() {
    // WebSocket功能已禁用
    console.log('WebSocket功能已禁用，使用HTTP轮询模式');
    isConnected = true; // 直接设置为连接状态
    updateConnectionStatus(true);
    addLog('系统', '系统已连接 (HTTP模式)');
}

/**
 * 启动轮询模式
 */
function startPollingMode() {
    // 立即更新一次状态
    updateSystemStatus();

    // 设置定时轮询（每3秒更新一次）
    setInterval(() => {
        updateSystemStatus();
    }, 3000);

    addLog('系统', '状态轮询已启动（每3秒更新）');
}

/**
 * 更新系统状态（轮询模式）
 */
function updateSystemStatus() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            if (data.code === 0) {
                // 确保连接状态为在线
                if (!isConnected) {
                    isConnected = true;
                    updateConnectionStatus(true);
                    addLog('系统', '连接已恢复');
                }

                // 更新完整的系统状态
                updateSystemStatusDisplay(data.data);
            }
        })
        .catch(error => {
            console.error('状态更新失败:', error);
            // 连接失败时更新状态
            if (isConnected) {
                isConnected = false;
                updateConnectionStatus(false);
                addLog('错误', '连接已断开');
            }
        });
}

/**
 * 更新系统状态显示（从轮询数据）
 */
function updateSystemStatusDisplay(data) {
    // 更新当前通道
    if (data.current_channel !== undefined) {
        currentChannel = data.current_channel;
        updateChannelDisplay();
    }

    // 更新WiFi状态
    if (data.wifi_status) {
        const wifiElement = document.getElementById('wifi-status');
        if (wifiElement) {
            wifiElement.textContent = data.wifi_status.connected ? '已连接' : '未连接';
        }
    }

    // 更新IP地址
    if (data.ip_address) {
        const ipElement = document.getElementById('ip-address');
        if (ipElement) {
            ipElement.textContent = data.ip_address;
        }
    }

    // 更新通信状态
    if (data.comm_status) {
        const commStatus = data.comm_status.connected ? '正常' : '断开';
        const statusElement = document.getElementById('comm-status');
        if (statusElement) {
            statusElement.textContent = commStatus;
        }
    }

    // 更新运行时间
    if (data.uptime) {
        const uptimeElement = document.getElementById('uptime');
        if (uptimeElement) {
            uptimeElement.textContent = formatUptime(data.uptime);
        }
    }

    // 更新统计信息
    if (data.stats) {
        updateStatsDisplay(data.stats);
    }

    // 更新通道状态
    if (data.channels) {
        updateChannelStatus(data.channels);
    }
}

/**
 * 更新统计信息显示
 */
function updateStatsDisplay(stats) {
    if (stats.total_switches !== undefined) {
        const element = document.getElementById('total-switches');
        if (element) element.textContent = stats.total_switches;
    }

    if (stats.error_count !== undefined) {
        const element = document.getElementById('error-count');
        if (element) element.textContent = stats.error_count;
    }

    if (stats.total_switches > 0) {
        const successRate = ((stats.total_switches - stats.error_count) / stats.total_switches * 100).toFixed(1);
        const element = document.getElementById('success-rate');
        if (element) element.textContent = `${successRate}%`;
    }

    if (stats.last_switch_time) {
        const lastSwitch = new Date(stats.last_switch_time * 1000);
        const element = document.getElementById('last-switch');
        if (element) element.textContent = lastSwitch.toLocaleString();
    }
}

/**
 * 处理WebSocket消息
 */
function handleWebSocketMessage(data) {
    switch (data.type) {
        case 'status_update':
            updateSystemStatus(data.data);
            break;
        case 'channel_switched':
            handleChannelSwitched(data.data);
            break;
        case 'error':
            showMessage(data.message, 'error');
            addLog('错误', data.message);
            break;
        default:
            console.log('未知WebSocket消息类型:', data.type);
    }
}

/**
 * 切换HDMI通道
 */
async function switchChannel(channel) {
    if (!isValidChannel(channel)) {
        showMessage('无效的通道号', 'error');
        return;
    }
    
    if (channel === currentChannel) {
        showMessage('已经是当前通道', 'info');
        return;
    }
    
    showLoading(true);
    addLog('操作', `正在切换到通道 ${channel}...`);
    
    try {
        const response = await fetch(`${API.SWITCH}/${channel}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        });
        
        const result = await response.json();
        
        if (result.code === 0) {
            // 切换成功
            currentChannel = channel;
            updateChannelDisplay();
            showMessage(`成功切换到通道 ${channel}`, 'success');
            addLog('操作', `成功切换到通道 ${channel}`);
        } else {
            // 切换失败
            showMessage(`切换失败: ${result.message}`, 'error');
            addLog('错误', `切换到通道 ${channel} 失败: ${result.message}`);
        }
        
    } catch (error) {
        console.error('切换通道失败:', error);
        showMessage('网络错误，切换失败', 'error');
        addLog('错误', `网络错误: ${error.message}`);
    } finally {
        showLoading(false);
    }
}

/**
 * 刷新系统状态
 */
async function refreshStatus() {
    showLoading(true);
    addLog('操作', '正在刷新系统状态...');
    
    try {
        const response = await fetch(API.STATUS);
        const result = await response.json();
        
        if (result.code === 0) {
            updateSystemStatus(result.data);
            showMessage('状态刷新成功', 'success');
            addLog('操作', '系统状态刷新完成');
        } else {
            showMessage(`状态刷新失败: ${result.message}`, 'error');
            addLog('错误', `状态刷新失败: ${result.message}`);
        }
        
    } catch (error) {
        console.error('刷新状态失败:', error);
        showMessage('网络错误，刷新失败', 'error');
        addLog('错误', `状态刷新失败: ${error.message}`);
    } finally {
        showLoading(false);
    }
}

/**
 * 更新系统状态显示
 */
function updateSystemStatus(data) {
    // 更新当前通道
    if (data.current_channel) {
        currentChannel = data.current_channel;
        updateChannelDisplay();
    }
    
    // 更新WiFi状态
    if (data.wifi_status) {
        document.getElementById('wifi-status').textContent = 
            data.wifi_status.connected ? '已连接' : '未连接';
    }
    
    // 更新IP地址
    if (data.ip_address) {
        document.getElementById('ip-address').textContent = data.ip_address;
    }
    
    // 更新通信状态
    if (data.comm_status) {
        document.getElementById('comm-status').textContent = 
            data.comm_status.connected ? '正常' : '断开';
    }
    
    // 更新统计信息
    if (data.stats) {
        document.getElementById('total-switches').textContent = data.stats.total_switches || 0;
        document.getElementById('error-count').textContent = data.stats.error_count || 0;
        
        const successRate = data.stats.total_switches > 0 ? 
            ((data.stats.total_switches - data.stats.error_count) / data.stats.total_switches * 100).toFixed(1) : 100;
        document.getElementById('success-rate').textContent = `${successRate}%`;
        
        if (data.stats.last_switch_time) {
            const lastSwitch = new Date(data.stats.last_switch_time * 1000);
            document.getElementById('last-switch').textContent = lastSwitch.toLocaleString();
        }
    }
    
    // 更新运行时间
    if (data.uptime) {
        document.getElementById('uptime').textContent = formatUptime(data.uptime);
    }
    
    // 更新通道状态
    if (data.channels) {
        updateChannelStatus(data.channels);
    }
}

/**
 * 更新通道显示
 */
function updateChannelDisplay() {
    // 移除所有active类
    document.querySelectorAll('.channel-card').forEach(card => {
        card.classList.remove('active');
    });
    
    // 添加当前通道的active类
    const currentCard = document.querySelector(`[data-channel="${currentChannel}"]`);
    if (currentCard) {
        currentCard.classList.add('active');
    }
    
    // 更新顶部状态栏
    document.getElementById('current-channel').textContent = currentChannel;
}

/**
 * 更新通道状态
 */
function updateChannelStatus(channels) {
    for (let i = 1; i <= 2; i++) {
        const statusElement = document.getElementById(`status-${i}`);
        const nameElement = document.getElementById(`name-${i}`);

        if (channels[i-1]) {
            const channel = channels[i-1];

            // 更新连接状态
            statusElement.className = `channel-status ${channel.connected ? 'connected' : 'disconnected'}`;

            // 更新通道名称
            if (channel.name && channel.name !== `电脑${i}`) {
                nameElement.textContent = channel.name;
            }
        }
    }
}

/**
 * 更新连接状态
 */
function updateConnectionStatus(connected) {
    const statusElement = document.getElementById('connection-status');
    if (connected) {
        statusElement.textContent = '在线';
        statusElement.className = 'status-value online';
    } else {
        statusElement.textContent = '离线';
        statusElement.className = 'status-value offline';
    }
}

/**
 * 处理通道切换完成事件
 */
function handleChannelSwitched(data) {
    currentChannel = data.channel;
    updateChannelDisplay();
    addLog('系统', `通道已切换到 ${data.channel}`);
}

/**
 * 开始状态更新定时器
 */
function startStatusUpdate() {
    // 立即执行一次
    refreshStatus();

    // 每30秒更新一次状态
    statusUpdateInterval = setInterval(refreshStatus, 30000);
}

/**
 * 停止状态更新定时器
 */
function stopStatusUpdate() {
    if (statusUpdateInterval) {
        clearInterval(statusUpdateInterval);
        statusUpdateInterval = null;
    }
}

/**
 * 初始化界面
 */
function updateUI() {
    updateChannelDisplay();
    updateConnectionStatus(false);
}

/**
 * 添加键盘快捷键
 */
function addKeyboardShortcuts() {
    document.addEventListener('keydown', function(event) {
        // 数字键1-2切换通道
        if (event.key >= '1' && event.key <= '2') {
            const channel = parseInt(event.key);
            switchChannel(channel);
            event.preventDefault();
        }

        // F5刷新状态
        if (event.key === 'F5') {
            refreshStatus();
            event.preventDefault();
        }

        // Escape关闭模态框
        if (event.key === 'Escape') {
            hideAbout();
            hideMessage();
        }
    });
}

/**
 * 验证通道号是否有效
 */
function isValidChannel(channel) {
    return channel >= 1 && channel <= 2;
}

/**
 * 格式化运行时间
 */
function formatUptime(seconds) {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;

    if (days > 0) {
        return `${days}天 ${hours}小时 ${minutes}分钟`;
    } else if (hours > 0) {
        return `${hours}小时 ${minutes}分钟`;
    } else if (minutes > 0) {
        return `${minutes}分钟 ${secs}秒`;
    } else {
        return `${secs}秒`;
    }
}

/**
 * 显示加载提示
 */
function showLoading(show) {
    const loadingElement = document.getElementById('loading');
    if (show) {
        loadingElement.classList.remove('hidden');
    } else {
        loadingElement.classList.add('hidden');
    }
}

/**
 * 显示消息提示
 */
function showMessage(message, type = 'info') {
    const messageElement = document.getElementById('message');
    const messageText = document.getElementById('message-text');

    messageText.textContent = message;
    messageElement.classList.remove('hidden');

    // 根据类型设置样式
    const messageContent = messageElement.querySelector('.message-content');
    messageContent.className = `message-content message-${type}`;

    // 3秒后自动隐藏
    setTimeout(hideMessage, 3000);
}

/**
 * 隐藏消息提示
 */
function hideMessage() {
    const messageElement = document.getElementById('message');
    messageElement.classList.add('hidden');
}

/**
 * 添加日志条目
 */
function addLog(category, message) {
    const timestamp = new Date().toLocaleTimeString();
    const logEntry = {
        time: timestamp,
        category: category,
        message: message
    };

    logEntries.unshift(logEntry);

    // 限制日志条目数量
    if (logEntries.length > 100) {
        logEntries = logEntries.slice(0, 100);
    }

    updateLogDisplay();
}

/**
 * 更新日志显示
 */
function updateLogDisplay() {
    const logContent = document.getElementById('log-content');

    logContent.innerHTML = logEntries.map(entry =>
        `<div class="log-entry">
            <span class="log-time">[${entry.time}] ${entry.category}:</span>
            <span class="log-message">${entry.message}</span>
        </div>`
    ).join('');

    // 滚动到顶部显示最新日志
    logContent.scrollTop = 0;
}

/**
 * 清空日志
 */
function clearLog() {
    logEntries = [];
    updateLogDisplay();
    addLog('系统', '日志已清空');
}

/**
 * 显示关于对话框
 */
function showAbout() {
    const modal = document.getElementById('about-modal');
    modal.classList.remove('hidden');
}

/**
 * 隐藏关于对话框
 */
function hideAbout() {
    const modal = document.getElementById('about-modal');
    modal.classList.add('hidden');
}
