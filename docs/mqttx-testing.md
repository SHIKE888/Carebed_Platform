# 使用 MQTTX 测试 CareBed 设备通道

本文档说明如何借助 [MQTTX](https://mqttx.app/) 验证 CareBed 平台的设备心跳与远程指令链路。示例默认基于 `application.yml` 中的内置 Broker 配置（主机 `127.0.0.1`，端口 `1883`，无鉴权）。若已将 Broker 暴露到其他地址/端口，请按实际环境替换。

## 1. 安装与准备
- 下载安装 MQTTX（桌面版或 CLI 均可）。下文以桌面版为例。
- 确保 CareBed 后端已启动，并启用 MQTT 功能（`carebed.mqtt.enabled=true`）。
- 准备一个已在系统登记的设备编码（例如 `BED-0001`），以及其数据库内的设备 UUID（可通过 `/api/devices` 查询）。

## 2. 建立连接
1. 打开 MQTTX，点击 **New Connection**。
2. 关键字段填写：
   - **Name**：`CareBed Local`
   - **Client ID**：随意（如 `mqttx-lab`）。
  - **Host**：`127.0.0.1`
  - **Port**：`1883`
  - **Protocol**：`MQTT/TCP`
  - **Protocol Version**：**3.1.1**（Moquette 默认仅支持 3.1.x；若选择 MQTT v5 会出现 `Connection refused: Unacceptable protocol version`）
   - **Username/Password**：留空（如启用鉴权则填入 `carebed.mqtt.broker` 配置）。
3. 保存并点击 **Connect**。状态指示灯变为绿色表示连接成功。

## 3. 监听指令 Topic
1. 在已连接的会话中点击 **+ New Subscription**。
2. Topic 填写：`devices/BED-0001/command`（将 `BED-0001` 替换为目标设备编码）。
3. QoS 选择 **2**（平台下发命令固定为 QoS 2）。
4. 点击 **Confirm**，观察订阅面板等待指令消息。

## 4. 触发远程指令
1. 通过后端 API 调用指令接口（需先登录获取 Bearer Token）。示例命令：
   ```bash
   curl -X POST \
     http://localhost:8081/api/devices/{deviceId}/unlock \
     -H "Authorization: Bearer <TOKEN>" -H "Content-Type: application/json"
   ```
   或者：
   ```bash
   curl -X POST \
     http://localhost:8081/api/devices/{deviceId}/reboot \
     -H "Authorization: Bearer <TOKEN>"
   ```
   将 `{deviceId}` 替换为设备 UUID。
2. 回到 MQTTX 的订阅面板，应收到如下 JSON（字段示例）：
   ```json
   {
     "command": "UNLOCK",          // 或 REBOOT
     "issuedAt": "2026-02-16T03:35:27.512Z",
     "commandId": "3fd73a5d-b0fc-4c41-a7e7-f3ad3e77a932"
   }
   ```
3. 检查 `commandId` 是否变化，重复下发时若设备端缓存最新 commandId，可避免重复执行。

## 5. 模拟设备心跳
1. 在 MQTTX 中点击 **+ New Publication**。
2. 填写：
   - **Topic**：`devices/BED-0001/heartbeat`
   - **QoS**：1（默认桥接 QoS，可按需调整）
   - **Payload**：
     ```json
     {
       "battery": 87,
       "lockStatus": "LOCKED"
     }
     ```
   - **Retain**：可选。建议关闭以模拟实时心跳。
3. 点击 **Publish**。若后端收到心跳，`/api/devices` 查询中对应设备应更新电量、锁状态与在线状态。

## 6. 模拟设备端执行反馈（可选）
- 若需要验证前端日志面板，可以在 MQTTX 中再次发布心跳，将 `lockStatus` 变更为 `UNLOCKED`，模拟执行完毕后的状态同步。
- 亦可在 MQTTX CLI 中循环发送心跳脚本，模拟稳定在线设备。

## 7. 常见排查
| 现象 | 处理建议 |
| --- | --- |
| 订阅无消息 | 确认 REST 接口调用成功；检查 Broker 与后端是否在同一主机，或 `carebed.mqtt.bridge.host` 是否指向 MQTTX 连接的 Broker。 |
| 发布心跳后后端无更新 | 验证 Topic 拼写是否与设备编码一致；确认 Payload JSON 中 `battery` 为 0-100 整数、`lockStatus` 为 `LOCKED`/`UNLOCKED`。 |
| 指令重复执行 | 设备应缓存 `commandId`，若测试需要可清空缓存后重试；MQTTX 订阅默认会保留历史消息，实际设备应只处理最新 commandId。 |
| 连接失败 | 检查端口占用、防火墙；若使用容器部署，请将 Broker 端口映射到宿主机，并在 MQTTX 中使用对应 IP。 |
| `Connection refused: Unacceptable protocol version` | 在 MQTTX 连接设置中将 **Protocol Version** 固定为 **3.1.1**；Moquette 暂不支持 MQTT v5。 |

通过以上步骤，即可用 MQTTX 完成心跳上报与远程指令链路的端到端测试。若需要批量脚本或自动化测试，可改用 MQTTX CLI 或其他 MQTT 客户端（如 mosquitto_pub/sub）。
