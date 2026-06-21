#ifndef CONFIG_H
#define CONFIG_H

// ==================== WiFi 配置 ====================
#define WIFI_SSID       "518"
#define WIFI_PASSWORD   "12345678"
#define WIFI_TIMEOUT    20000  // 连接超时 (ms)

// ==================== MQTT 配置 ====================
#define MQTT_SERVER     "123.207.45.73"
#define MQTT_PORT       1883
#define MQTT_USER       "123456"
#define MQTT_PASS       "qazws123456"
#define MQTT_CLIENT_ID  "ESP32_GreenHouse"

// MQTT 主题
#define TOPIC_DATA      "greenhouse/sensor/data"
#define TOPIC_CTRL      "greenhouse/control"
#define TOPIC_STATUS    "greenhouse/status"
#define TOPIC_AI        "greenhouse/ai/result"

// ==================== 引脚定义 ====================
// OLED (I2C)
#define OLED_SDA        8
#define OLED_SCL        9
#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_ADDR       0x3C

// 按键
#define BTN_PIN         41

// LED
#define LED_PIN         48

// STM32 通信 (UART2)
#define STM32_RX        17
#define STM32_TX        18
#define STM32_BAUD      115200

// 调试串口 (UART1)
#define DBG_RX          45
#define DBG_TX          46
#define DBG_BAUD        115200

// ==================== 定时器配置 ====================
#define OLED_UPDATE_INTERVAL    2000    // OLED 更新间隔 (ms)
#define MQTT_PUBLISH_INTERVAL   10000   // 数据发布间隔 (ms)
#define HEARTBEAT_INTERVAL      30000   // 心跳间隔 (ms)
#define MQTT_RECONNECT_INTERVAL 5000    // MQTT 重连间隔 (ms)

// ==================== 数据结构 ====================
struct SensorData {
    int node_id;
    float temp;
    float humi;
    int lux;
    int water;
    int air;
    String cls;      // AI 识别类别
    float conf;      // AI 置信度
    int pest;        // 病虫害检测
    unsigned long timestamp;
};

#endif // CONFIG_H
