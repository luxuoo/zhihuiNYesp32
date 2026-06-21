#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <U8g2lib.h>
#include "config.h"

// ==================== 全局对象 ====================
// SH1106 OLED 显示屏
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

WiFiClient espClient;
PubSubClient mqtt(espClient);
HardwareSerial SerialSTM32(2);  // UART2 用于 STM32 通信

// ==================== 数据结构 ====================
SensorData node1Data = {0};
SensorData node2Data = {0};
SensorData aiResult  = {0};

// ==================== 状态变量 ====================
unsigned long lastMqttReconnect = 0;
unsigned long lastOledUpdate = 0;
unsigned long lastHeartbeat = 0;
bool wifiConnected = false;
bool mqttConnected = false;
int displayPage = 0;
String inputBuffer = "";

// ==================== 函数声明 ====================
void setupWiFi();
void setupMQTT();
void setupOLED();
void setupUART();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void processSTM32Data(String data);
void processAIData(String data);
void publishSensorData();
void updateOLED();
void drawPage0();  // 状态页
void drawPage1();  // 节点1数据
void drawPage2();  // 节点2数据
void drawPage3();  // AI结果
void checkButton();
void heartbeat();
void displayCenterMessage(const char* msg);

// ==================== 初始化 ====================
void setup() {
  // 调试串口
  Serial.begin(115200);
  delay(100);

  // 引脚初始化
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("\n[ESP32] 智能温室种植系统 - 网关启动");

  // 初始化各模块
  setupOLED();
  setupUART();
  setupWiFi();
  setupMQTT();

  Serial.println("[ESP32] 初始化完成");
}

// ==================== 主循环 ====================
void loop() {
  // 维持 MQTT 连接
  if (!mqtt.connected()) {
    reconnectMQTT();
  }
  mqtt.loop();

  // 读取 STM32 数据
  while (SerialSTM32.available()) {
    char c = SerialSTM32.read();
    if (c == '\n') {
      if (inputBuffer.length() > 0) {
        processSTM32Data(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }

  // 读取调试串口数据 (MaixCAM AI结果)
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      // 可用于接收 MaixCAM 数据
      inputBuffer = "";
    }
  }

  // 定时更新 OLED
  if (millis() - lastOledUpdate > 2000) {
    updateOLED();
    lastOledUpdate = millis();
  }

  // 定时发布数据到 MQTT
  if (millis() - lastHeartbeat > 10000) {
    publishSensorData();
    heartbeat();
    lastHeartbeat = millis();
  }

  // 检测按键
  checkButton();
}

// ==================== OLED 初始化 ====================
void setupOLED() {
  Serial.println("[OLED] Initializing...");

  // 初始化 I2C
  Wire.begin(OLED_SDA, OLED_SCL);

  // 初始化 U8g2
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);  // 中文字体

  // 清屏并显示启动信息
  displayCenterMessage("Smart Greenhouse");
  delay(1000);

  displayCenterMessage("Gateway v1.0");
  delay(1000);

  displayCenterMessage("Initializing...");
  delay(500);

  Serial.println("[OLED] Init OK!");
}

// ==================== 显示居中消息 ====================
void displayCenterMessage(const char* msg) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  int textWidth = u8g2.getUTF8Width(msg);
  u8g2.drawUTF8((128 - textWidth) / 2, 36, msg);
  u8g2.sendBuffer();
}

// ==================== WiFi 初始化 ====================
void setupWiFi() {
  displayCenterMessage("连接WiFi中...");
  delay(500);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());

    String ipMsg = "IP: " + WiFi.localIP().toString();
    displayCenterMessage(ipMsg.c_str());
    delay(1000);
  } else {
    Serial.println("\n[WiFi] Failed!");
    displayCenterMessage("WiFi连接失败!");
    delay(1000);
  }
}

// ==================== MQTT 初始化 ====================
void setupMQTT() {
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  reconnectMQTT();
}

void reconnectMQTT() {
  if (millis() - lastMqttReconnect < 5000) return;
  lastMqttReconnect = millis();

  if (!wifiConnected) return;

  Serial.print("[MQTT] Connecting...");
  if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
    mqttConnected = true;
    Serial.println("Connected!");

    // 订阅控制主题
    mqtt.subscribe(TOPIC_CTRL);
    mqtt.subscribe(TOPIC_AI);

    // 发布上线消息
    mqtt.publish(TOPIC_STATUS, "{\"status\":\"online\",\"device\":\"ESP32_Gateway\"}");
  } else {
    Serial.print("Failed, rc=");
    Serial.println(mqtt.state());
  }
}

// ==================== MQTT 回调 ====================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("[MQTT] Received: ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(message);

  String topicStr = String(topic);

  // 控制命令
  if (topicStr == TOPIC_CTRL) {
    // 转发给 STM32
    SerialSTM32.println(message);
    Serial.println("[MQTT] Forwarded to STM32");
  }

  // AI 识别结果
  if (topicStr == TOPIC_AI) {
    processAIData(message);
  }
}

// ==================== UART 初始化 ====================
void setupUART() {
  // STM32 通信
  SerialSTM32.begin(115200, SERIAL_8N1, STM32_RX, STM32_TX);
  Serial.println("[UART] STM32 UART initialized");
}

// ==================== 处理 STM32 数据 ====================
void processSTM32Data(String data) {
  Serial.print("[STM32] Received: ");
  Serial.println(data);

  // 解析 JSON: {"node":1,"temp":25.3,"humi":48,"lux":180}
  // 或: {"node":2,"water":1200,"air":1}
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, data);

  if (error) {
    Serial.print("[JSON] Parse error: ");
    Serial.println(error.c_str());
    return;
  }

  int node = doc["node"] | 0;

  if (node == 1) {
    node1Data.node_id = 1;
    node1Data.temp = doc["temp"] | 0.0f;
    node1Data.humi = doc["humi"] | 0.0f;
    node1Data.lux  = doc["lux"] | 0;
    node1Data.timestamp = millis();
    Serial.printf("[Node1] T=%.1f H=%.1f L=%d\n", node1Data.temp, node1Data.humi, node1Data.lux);
  }
  else if (node == 2) {
    node2Data.node_id = 2;
    node2Data.water = doc["water"] | 0;
    node2Data.air   = doc["air"] | 0;
    node2Data.timestamp = millis();
    Serial.printf("[Node2] W=%d A=%d\n", node2Data.water, node2Data.air);
  }
}

// ==================== 处理 AI 数据 ====================
void processAIData(String data) {
  Serial.print("[AI] Received: ");
  Serial.println(data);

  // 解析: {"cls":"healthy","conf":0.93,"pest":0}
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, data);

  if (error) {
    Serial.print("[JSON] Parse error: ");
    Serial.println(error.c_str());
    return;
  }

  aiResult.cls  = doc["cls"] | "unknown";
  aiResult.conf = doc["conf"] | 0.0f;
  aiResult.pest = doc["pest"] | 0;
  aiResult.timestamp = millis();

  Serial.printf("[AI] Class=%s Conf=%.2f Pest=%d\n",
                aiResult.cls.c_str(), aiResult.conf, aiResult.pest);
}

// ==================== 发布传感器数据 ====================
void publishSensorData() {
  if (!mqttConnected) return;

  // 发布节点1数据
  if (node1Data.timestamp > 0) {
    StaticJsonDocument<256> doc1;
    doc1["node"] = 1;
    doc1["temp"] = serialized(String(node1Data.temp, 1));
    doc1["humi"] = serialized(String(node1Data.humi, 1));
    doc1["lux"]  = node1Data.lux;
    doc1["time"] = millis();

    char buffer1[256];
    serializeJson(doc1, buffer1);
    mqtt.publish(TOPIC_DATA, buffer1);
  }

  // 发布节点2数据
  if (node2Data.timestamp > 0) {
    StaticJsonDocument<256> doc2;
    doc2["node"] = 2;
    doc2["water"] = node2Data.water;
    doc2["air"]   = node2Data.air;
    doc2["time"]  = millis();

    char buffer2[256];
    serializeJson(doc2, buffer2);
    mqtt.publish(TOPIC_DATA, buffer2);
  }
}

// ==================== 心跳 ====================
void heartbeat() {
  if (!mqttConnected) return;

  StaticJsonDocument<128> doc;
  doc["device"] = "ESP32_Gateway";
  doc["uptime"] = millis() / 1000;
  doc["wifi"]   = wifiConnected;
  doc["mqtt"]   = mqttConnected;
  doc["rssi"]   = WiFi.RSSI();

  char buffer[128];
  serializeJson(doc, buffer);
  mqtt.publish(TOPIC_STATUS, buffer);
}

// ==================== 更新 OLED ====================
void updateOLED() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);

  switch (displayPage) {
    case 0: drawPage0(); break;
    case 1: drawPage1(); break;
    case 2: drawPage2(); break;
    case 3: drawPage3(); break;
    default: drawPage0(); break;
  }

  u8g2.sendBuffer();
}

// 页面0: 状态页
void drawPage0() {
  char buffer[64];

  // 标题
  u8g2.drawUTF8(30, 12, "系统状态");
  u8g2.drawHLine(0, 18, 128);

  // WiFi 状态
  u8g2.drawUTF8(5, 32, "WiFi:");
  u8g2.drawUTF8(60, 32, wifiConnected ? "已连接" : "未连接");

  // MQTT 状态
  u8g2.drawUTF8(5, 46, "MQTT:");
  u8g2.drawUTF8(60, 46, mqttConnected ? "已连接" : "未连接");

  // 运行时间
  snprintf(buffer, sizeof(buffer), "运行: %lus", millis() / 1000);
  u8g2.drawUTF8(5, 60, buffer);

  // 页码
  snprintf(buffer, sizeof(buffer), "[%d/3]", displayPage);
  u8g2.drawUTF8(100, 60, buffer);
}

// 页面1: 节点1数据 (温湿度+光照)
void drawPage1() {
  char buffer[64];

  // 标题
  u8g2.drawUTF8(20, 12, "节点1-环境");
  u8g2.drawHLine(0, 18, 128);

  // 温度
  u8g2.drawUTF8(5, 32, "温度:");
  snprintf(buffer, sizeof(buffer), "%.1f C", node1Data.temp);
  u8g2.drawUTF8(60, 32, buffer);

  // 湿度
  u8g2.drawUTF8(5, 46, "湿度:");
  snprintf(buffer, sizeof(buffer), "%.1f %%", node1Data.humi);
  u8g2.drawUTF8(60, 46, buffer);

  // 光照
  u8g2.drawUTF8(5, 60, "光照:");
  snprintf(buffer, sizeof(buffer), "%d lx", node1Data.lux);
  u8g2.drawUTF8(60, 60, buffer);

  // 页码
  snprintf(buffer, sizeof(buffer), "[%d/3]", displayPage);
  u8g2.drawUTF8(100, 12, buffer);
}

// 页面2: 节点2数据 (水位+空气质量)
void drawPage2() {
  char buffer[64];

  // 标题
  u8g2.drawUTF8(20, 12, "节点2-水气");
  u8g2.drawHLine(0, 18, 128);

  // 水位
  u8g2.drawUTF8(5, 32, "水位:");
  snprintf(buffer, sizeof(buffer), "%d mm", node2Data.water);
  u8g2.drawUTF8(60, 32, buffer);

  // 空气质量
  u8g2.drawUTF8(5, 46, "空气:");
  u8g2.drawUTF8(60, 46, node2Data.air == 0 ? "良好" : "较差");

  // 页码
  snprintf(buffer, sizeof(buffer), "[%d/3]", displayPage);
  u8g2.drawUTF8(100, 12, buffer);
}

// 页面3: AI 识别结果
void drawPage3() {
  char buffer[64];

  // 标题
  u8g2.drawUTF8(20, 12, "AI识别结果");
  u8g2.drawHLine(0, 18, 128);

  // 类别
  u8g2.drawUTF8(5, 32, "状态:");
  u8g2.drawUTF8(60, 32, aiResult.cls.c_str());

  // 置信度
  u8g2.drawUTF8(5, 46, "置信:");
  snprintf(buffer, sizeof(buffer), "%.1f%%", aiResult.conf * 100);
  u8g2.drawUTF8(60, 46, buffer);

  // 病虫害
  u8g2.drawUTF8(5, 60, "病虫:");
  u8g2.drawUTF8(60, 60, aiResult.pest == 0 ? "无" : "有");

  // 页码
  snprintf(buffer, sizeof(buffer), "[%d/3]", displayPage);
  u8g2.drawUTF8(100, 12, buffer);
}

// ==================== 按键检测 ====================
void checkButton() {
  static bool lastState = HIGH;
  static unsigned long lastDebounce = 0;

  bool currentState = digitalRead(BTN_PIN);

  if (currentState != lastState) {
    lastDebounce = millis();
  }

  if ((millis() - lastDebounce) > 50) {
    if (currentState == LOW && lastState == HIGH) {
      // 按键按下，切换页面
      displayPage = (displayPage + 1) % 4;
      updateOLED();
      Serial.printf("[BTN] Page: %d\n", displayPage);
    }
  }

  lastState = currentState;
}
