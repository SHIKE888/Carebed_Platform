# MQTT Broker Integration Guide

## 当前实现
- `io.moquette:moquette-broker` 与 `org.eclipse.paho:org.eclipse.paho.client.mqttv3` 已加入 [Software/SpringBoot/pom.xml](../Software/SpringBoot/pom.xml)。
- 新增 [Software/SpringBoot/src/main/java/com/carebed/config/MqttBrokerConfig.java](../Software/SpringBoot/src/main/java/com/carebed/config/MqttBrokerConfig.java) 启动内嵌 Broker，默认监听 `0.0.0.0:1883`，持久化存储在 `mqtt-data`。
- 桥接逻辑由 [Software/SpringBoot/src/main/java/com/carebed/device/mqtt/DeviceMqttBridge.java](../Software/SpringBoot/src/main/java/com/carebed/device/mqtt/DeviceMqttBridge.java) 负责：
  - 订阅 `devices/+/heartbeat`，解析心跳包并更新 `DeviceService`；
  - 提供 `sendUnlockCommand`，通过 `devices/{deviceCode}/command` 下发行为 `{"command":"UNLOCK"}`；
  - 定时扫描心跳超时设备，自动标记离线。
- 设备状态与事件现存储于 MySQL（`devices`、`device_events` 表），服务重启后仍可追踪历史心跳与操作记录。
- 设备模型扩展在线状态与锁状态：见 [Device](../Software/SpringBoot/src/main/java/com/carebed/device/Device.java) 与 [DeviceResponse](../Software/SpringBoot/src/main/java/com/carebed/device/dto/DeviceResponse.java)。
- 新增 REST 接口 `POST /api/devices/{id}/unlock`，通过 MQTT 执行开锁；心跳接口 `POST /api/devices/{id}/heartbeat` 现要求 `lockStatus`。
- 应用启用调度任务处理超时：见 [CarebedApplication](../Software/SpringBoot/src/main/java/com/carebed/CarebedApplication.java)。

## 配置项
默认配置位于 [Software/SpringBoot/src/main/resources/application.yml](../Software/SpringBoot/src/main/resources/application.yml)：

```yaml
carebed:
  mqtt:
    enabled: true
    broker:
      host: 0.0.0.0
      port: 1883
      persistence-path: mqtt-data
    bridge:
      host: 127.0.0.1
      port: 1883
      qos: 1
    heartbeat-timeout-ms: 90000
    offline-check-interval-ms: 30000
```

- `heartbeat-timeout-ms`：心跳超时时间，超时后设备被置为 `OFFLINE` 且在线状态变为 `OFFLINE`。
- `offline-check-interval-ms`：巡检周期，定时触发离线检测。

## MQTT Topic 协议
- 上行心跳：`devices/{deviceCode}/heartbeat`
  - Payload（JSON）：
    ```json
    {
      "battery": 85,
      "lockStatus": "LOCKED"
    }
    ```
  - 处理逻辑：更新电量、锁状态，并将在线状态置为 `ONLINE`。
- 下行命令：`devices/{deviceCode}/command`
  - Payload（JSON）：`{"command":"UNLOCK","issuedAt":"ISO-8601"}`
  - ESP32 收到后应执行开锁并在下一次心跳中回传最新锁状态。

## ESP32 参考实现
- 单文件示例位于 [Frimware/esp32-mqtt-reference.ino](../Frimware/esp32-mqtt-reference.ino)：
  - 使用 `PubSubClient` 连接内嵌 Broker，并订阅命令主题；
  - 每 30 秒发布心跳，包含电量与锁状态；
  - 收到 `UNLOCK` 指令后执行本地开锁逻辑并在下次心跳中回传状态。

## 设备状态逻辑
- 在线状态：`ONLINE` / `OFFLINE`，由心跳决定；超时自动转离线。
- 锁状态：`LOCKED` / `UNLOCKED`，来自心跳 payload。
- 业务状态（原 `DeviceStatus`）：
  - 设备离线时自动标记为 `OFFLINE`；
  - 心跳恢复后根据是否已绑定患者切换为 `IN_USE` 或 `AVAILABLE`（维护状态保留）。
- 所有变更均写入 `DeviceEvent`，供前端事件时间线展示。

## 前端对接建议
- 设备列表新增两列：在线状态与锁状态；`DeviceResponse` 已暴露 `onlineStatus`、`lockStatus`。
- 心跳详情取 `lastHeartbeat` 字段展示最近时间戳。
- 开锁按钮调用 `POST /api/devices/{id}/unlock`，操作成功提示“开锁指令已发送”，等待心跳回传锁状态确认。
- 若接口返回 `MQTT 功能未启用`，需提示管理员检查系统设置。

## 测试建议
- 单元：mock MQTT 消息校验 `DeviceMqttBridge` 心跳解析与异常处理。
- 集成：通过 Paho 测试客户端模拟 ESP32，验证上下行流程、状态切换与事件记录。
- 前端联调：重点校验租赁流程触发后开锁按钮可见性与反馈。

## 下一步
- 按需补充 MQTT 认证（当前默认允许匿名接入）。
- 若要部署独立 Broker，可切换 `carebed.mqtt.enabled=false` 并替换 `DeviceMqttBridge` 连接目标。
- 引入遥测扩展字段（如温湿度），沿用现有心跳主题即可。
