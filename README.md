# 智能温室种植系统 - ESP32 网关

## 项目概述

本项目是智能温室种植系统的 ESP32-S3 网关部分，负责：
- 接收 STM32 节点板的传感器数据（温湿度、光照、水位、空气质量）
- 通过 MQTT 上传数据到云平台
- 接收 MaixCAM 的 AI 识别结果（植物健康状态、病虫害检测）
- OLED 显示实时数据
- 按键切换显示页面

## 硬件配置

### ESP32-S3 引脚分配

| 功能 | 引脚 | 说明 |
|------|------|------|
| OLED SDA | GPIO8 | I2C 数据线 |
| OLED SCL | GPIO9 | I2C 时钟线 |
| 按键 | GPIO41 | 按键输入（内部上拉） |
| LED | GPIO48 | 状态指示灯 |
| STM32 RX | GPIO17 | UART2 接收 |
| STM32 TX | GPIO18 | UART2 发送 |
| 调试 RX | GPIO45 | UART1 接收 |
| 调试 TX | GPIO46 | UART1 发送 |

### 通信协议

#### STM32 -> ESP32 (UART, 115200 8N1)

**节点1 数据格式** (温湿度+光照):
```json
{"node":1,"temp":25.3,"humi":48,"lux":180}
```

**节点2 数据格式** (水位+空气质量):
```json
{"node":2,"water":1200,"air":1}
```

#### MaixCAM -> ESP32 (UART, 115200 8N1)

**AI 识别结果格式**:
```json
{"cls":"healthy","conf":0.93,"pest":0}
```

#### MQTT 主题

| 主题 | 方向 | 说明 |
|------|------|------|
| `greenhouse/sensor/data` | ESP32 -> 云 | 传感器数据 |
| `greenhouse/control` | 云 -> ESP32 | 控制命令 |
| `greenhouse/status` | ESP32 -> 云 | 设备状态 |
| `greenhouse/ai/result` | 云 -> ESP32 | AI 识别结果 |

## 软件依赖

- Adafruit SSD1306 (OLED 驱动)
- Adafruit GFX Library (图形库)
- ArduinoJson (JSON 解析)
- PubSubClient (MQTT 客户端)

## 配置说明

### 1. WiFi 配置

在 `main.cpp` 中修改：
```cpp
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

### 2. MQTT 配置

```cpp
const char* MQTT_SERVER   = "broker.emqx.io";  // MQTT 服务器地址
const char* MQTT_PORT     = "1883";              // 端口
const char* MQTT_USER     = "";                  // 用户名（可选）
const char* MQTT_PASS     = "";                  // 密码（可选）
```

## 编译和上传

1. 安装 PlatformIO IDE (VS Code 插件)
2. 打开项目文件夹
3. 修改 WiFi 和 MQTT 配置
4. 点击 PlatformIO 图标 -> Upload

## OLED 显示页面

按 GPIO41 按键切换页面：

- **Page 0**: 状态页 - WiFi/MQTT 连接状态、信号强度、运行时间
- **Page 1**: 节点1 - 温度、湿度、光照
- **Page 2**: 节点2 - 水位、空气质量
- **Page 3**: AI 结果 - 识别类别、置信度、病虫害状态

## 系统架构

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  节点板 1   │     │  节点板 2   │     │   MaixCAM   │
│ 温湿度+光照 │     │ 水位+空气   │     │  AI 识别    │
└──────┬──────┘     └──────┬──────┘     └──────┬──────┘
       │                   │                   │
       └───────────┬───────┘                   │
                   │                           │
            ┌──────┴──────┐                    │
            │   STM32     │                    │
            │  网关板     │                    │
            └──────┬──────┘                    │
                   │                           │
                   │ UART                      │ UART
                   │                           │
            ┌──────┴───────────────────────────┴──────┐
            │              ESP32-S3                    │
            │           WiFi/MQTT 网关                 │
            └──────────────────┬───────────────────────┘
                               │
                               │ WiFi
                               │
                        ┌──────┴──────┐
                        │  MQTT 服务器 │
                        │   (云平台)   │
                        └─────────────┘
```

## 故障排查

1. **OLED 不亮**
   - 检查 I2C 接线 (SDA=GPIO8, SCL=GPIO9)
   - 确认 OLED 地址为 0x3C

2. **WiFi 连接失败**
   - 检查 SSID 和密码
   - 确保信号强度足够

3. **MQTT 连接失败**
   - 检查服务器地址和端口
   - 确认网络可达

4. **STM32 数据不更新**
   - 检查 UART 接线 (RX=GPIO17, TX=GPIO18)
   - 确认波特率 115200

## License

MIT
