# ESP32 设备固件

本目录提供示例固件 `esp32-mqtt-reference.ino`，用于陪护床终端通过 MQTT 与后端交互（心跳上报、远程开锁/重启），并支持串口配置 WiFi 与设备编号。

## 功能概览

- WiFi 连接：从 EEPROM 读取并持久化 WiFi SSID/密码；串口命令可动态修改。
- MQTT 交互：
  - 心跳发布 `devices/{deviceCode}/heartbeat`，载荷示例 `{ "battery":100, "lockStatus":"LOCKED" }`，默认 10 秒一次。
  - 指令订阅 `devices/{deviceCode}/command`，支持 `UNLOCK`、`REBOOT`（带可选 `commandId` 去重）。
- 开锁策略：对继电器引脚输出脉冲，失败自动重试（默认最多 3 次），同时读取锁状态引脚验证。
- 串口配置：输入命令可查看/修改 WiFi 与设备编号，修改后自动保存到 EEPROM。

## 硬件接线

- 继电器/锁控输出：GPIO 4（高电平脉冲 `UNLOCK_PULSE_MS` 毫秒）。
- 锁状态检测：GPIO 34，默认上拉输入，下降沿中断检测状态变化。
- 需确保外设电平与 ESP32 兼容，并在继电器驱动端做好隔离。

## MQTT 主题与消息

- 心跳主题：`devices/{deviceCode}/heartbeat`（retain=true）。
- 指令主题：`devices/{deviceCode}/command`，QoS1 订阅，支持指令：
  - `{"command":"UNLOCK","commandId":"..."}`
  - `{"command":"REBOOT","commandId":"..."}`
- 心跳间隔：`HEARTBEAT_INTERVAL`（默认 10s），WiFi 连接或开锁后会立即补发一次。

## 串口命令

- `HELP`：查看命令帮助。
- `SHOW`：打印当前 SSID、密码、设备编号。
- `SET WIFI <ssid> <password>`：更新 WiFi 凭证并保存。
- `SET ID <deviceCode>`：更新设备编号并保存，MQTT 主题随之刷新。

## 编译与烧录

- 开发板：ESP32 DevKit 系列。
- 依赖库：`WiFi.h`、`PubSubClient`、`EEPROM`（均可通过 Arduino 库管理器获取或随核心提供）。
- 使用 Arduino IDE：选择正确串口与开发板，打开 `esp32-mqtt-reference.ino` 后直接编译上传。
- 使用 PlatformIO：创建 ESP32 平台工程，将源码置于 `src`，配置 `platformio.ini` 后上传。

## 运行流程

1. 上电后从 EEPROM 读取配置并连入指定 WiFi。
2. 连接 MQTT Broker（`MQTT_HOST/MQTT_PORT` 在源码顶部定义）。
3. 定期上报心跳，接收指令并执行开锁/重启；每次状态变化都会立即补发心跳。
4. 如需修改 WiFi 或设备编号，可通过串口输入命令，无需重新刷写固件。

## 故障排查

- WiFi 连接失败：串口查看日志，确认 SSID/密码正确后用 `SET WIFI` 更新；必要时重启设备。
- MQTT 未连接：确认 Broker 地址、端口、凭据以及网络可达性；修改源码或通过桥接调整。
- 开锁无效：检查 GPIO 4 硬件接线与供电，查看串口是否提示“Unlock failed after retries”。
