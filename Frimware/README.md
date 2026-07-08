# CareBed ESP32 设备固件

本目录包含两套固件，分别对应**传统 MQTT 直连模式**和**BLE Mesh 组网模式**。

---

## 固件选择指南

| 固件 | 目录 | 适用场景 | 通信方式 |
|------|------|----------|----------|
| esp32-mqtt-reference | esp32-mqtt-reference/ | 设备可直接连接 WiFi | MQTT 直连服务器 |
| esp32-mqtt-mesh | esp32-mqtt-mesh/ | 设备无法全部联网，需组网中继 | BLE Mesh + MQTT 网关桥接 |

---

## 目录结构

```
Frimware/
  README.md
  esp32-mqtt-reference/      # 传统 MQTT 直连固件
    esp32-mqtt-reference.ino
  esp32-mqtt-mesh/           # BLE Mesh 组网固件（推荐）
    esp32-mqtt-mesh.ino
  old/                       # 旧版固件（归档）
    esp32.ino
```

---

## 1. esp32-mqtt-mesh — BLE Mesh 组网固件（推荐）

### 功能概述

所有设备通过 BLE 构建自组织 mesh 网络，无需全部连接 WiFi：

- **主从一体 BLE**：每个 ESP32 同时作为 GATT Server（广播）和 GATT Client（扫描连接），实现全双工 mesh 通信。
- **智能网关**：连接 WiFi 成功的设备自动成为网关，通过 MQTT 桥接 mesh 与云端。
- **防泛洪转发**：每条消息携带唯一 msgId 和 TTL，128 条目 LRU 缓存去重，每节点只执行/转发一次。
- **动态 MQTT 订阅**：网关根据 mesh 成员列表自动订阅 devices/{deviceCode}/command 主题。
- **纯 BLE 终端**：非网关设备不连接 WiFi/MQTT，所有通信（心跳、指令、响应）通过 BLE mesh 完成。

### 硬件接线

| 引脚 | 功能 | 信号方向 |
|------|------|----------|
| GPIO 4 | 电磁锁继电器控制 | 输出 |
| GPIO 48 | 锁状态反馈（上拉输入，双向中断） | 输入 |
| GPIO 8 (SDA) | OLED I2C 数据线 | 双向 |
| GPIO 9 (SCL) | OLED I2C 时钟线 | 输出 |

### BLE Mesh 消息协议

每条 mesh 消息为 JSON 格式，结构如下：

```
{"i":"msgId","t":1,"s":"BED-0001","d":"BED-0002","h":5,"p":"{...}"}
```

| 字段 | 含义 | 说明 |
|------|------|------|
| i | 消息 ID | deviceCode-hexTime-seq，全局唯一 |
| t | 消息类型 | 1=心跳, 2=指令, 3=指令响应, 5=拓扑同步, 6=加入, 7=离开 |
| s | 源设备 | 发送方 deviceCode |
| d | 目标设备 | 仅 MSG_COMMAND 使用，ALL 为广播 |
| h | TTL | 初始 5，逐跳递减，0 时丢弃 |
| p | 载荷 | 嵌套 JSON，内容因类型而异 |

### 防泛洪机制

每条消息的 msgId 由源设备唯一生成。每个节点维护 128 条 LRU 缓存，检测到重复 msgId 时直接丢弃。TTL 每转发一次减 1，到达 0 后停止转发。

### 串口命令

| 命令 | 说明 |
|------|------|
| HELP | 查看帮助 |
| SHOW | 显示设备状态（编号、网关状态、peer 数、MQTT 状态） |
| PEERS | 列出已知 mesh 邻居 |
| SCAN | 手动触发 BLE 扫描 |
| UNLOCK | 测试开锁 |
| REBOOT | 重启设备 |
| SET WIFI ssid password | 设置 WiFi 凭证 |
| SET ID deviceCode | 设置设备编号 |

### 编译与烧录

- 开发板：ESP32 DevKit 系列
- 依赖库：BLEDevice, PubSubClient, U8g2lib, EEPROM
- 串口波特率：115200

---

## 2. esp32-mqtt-reference — MQTT 直连固件

适用于设备可直接连接 WiFi 的场景：

- WiFi 连接：从 EEPROM 读取并持久化 WiFi SSID/密码
- MQTT 交互：心跳发布 devices/{deviceCode}/heartbeat，指令订阅 devices/{deviceCode}/command
- 开锁策略：GPIO 4 脉冲输出（500ms），失败最多重试 3 次，通过 GPIO 48 验证锁状态

### 硬件接线

| 引脚 | 功能 | 信号方向 |
|------|------|----------|
| GPIO 4 | 电磁锁继电器控制 | 输出 |
| GPIO 48 | 锁状态反馈（上拉输入，双向中断） | 输入 |
| GPIO 8 (SDA) | OLED I2C 数据线 | 双向 |
| GPIO 9 (SCL) | OLED I2C 时钟线 | 输出 |

### 串口命令

HELP, SHOW, SCAN, SET WIFI ssid pwd, SET ID code

---

## 硬件通用说明

- 锁控输出：GPIO 4 到继电器到电磁锁（高电平脉冲驱动）
- 锁状态检测：GPIO 48，上拉输入，CHANGE 中断
- OLED 显示：I2C 地址 0x3C，SDA=GPIO 8，SCL=GPIO 9
- 需确保外设电平与 ESP32 兼容，继电器驱动端做好隔离
