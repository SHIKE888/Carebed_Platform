# CareBed 平台后端
CareBed 平台是面向医院共享陪护床业务的后端服务，基于 Spring Boot 3 与 Java 21 开发，覆盖从用户注册登录、设备运营、租借计费到报修、钱包、运营数据分析的完整能力。当前已使用 MySQL 持久化，并提供 MQTT 桥接、远程指令与收发日志留存。

---
- [CareBed 平台后端](#carebed-平台后端)
	- [快速开始](#快速开始)
	- [配置要点与运行](#配置要点与运行)
  - [系统概览](#系统概览)
  - [架构与技术栈](#架构与技术栈)
  - [未来规划](#未来规划)
  - [角色与权限矩阵](#角色与权限矩阵)
  - [功能模块详解](#功能模块详解)
    - [1. 用户与认证](#1-用户与认证)
    - [2. 智能设备管理与 MQTT](#2-智能设备管理与-mqtt)
    - [3. 租借使用与归还](#3-租借使用与归还)
    - [4. 消息与通知](#4-消息与通知)
    - [5. 报修与维修](#5-报修与维修)
    - [6. 我的钱包](#6-我的钱包)
    - [7. 平台综合管理](#7-平台综合管理)
  - [核心业务流程](#核心业务流程)
    - [租借流程（家属端）](#租借流程家属端)
    - [超时检测](#超时检测)
    - [报修流程](#报修流程)
    - [钱包争议处理](#钱包争议处理)
  - [数据模型速览](#数据模型速览)
  - [API 参考](#api-参考)
    - [认证与用户](#认证与用户)
    - [设备管理](#设备管理)
    - [租借归还](#租借归还)
    - [钱包申诉](#钱包申诉)
  - [通知策略](#通知策略)
  - [测试与质量保障](#测试与质量保障)
  - [运维与部署建议](#运维与部署建议)
  - [常见问题](#常见问题)

## 快速开始

1. 环境准备：JDK 21、Maven、MySQL 8（或兼容版本）以及可访问的 MQTT Broker。
2. 数据库：创建 `carebed` 库（或自定义），并在 `src/main/resources/application.yml` 中调整 `spring.datasource.url/username/password`。
3. MQTT：根据实际 Broker 修改 `carebed.mqtt.bridge.host/port`，若直接使用本地 Broker，请确保端口开放且凭据正确。
4. 运行：
	 - 开发模式：`mvn spring-boot:run`
	 - 打包：`mvn -DskipTests package` 后执行 `java -jar target/carebed-platform-0.0.1-SNAPSHOT.jar`
5. 验证：服务默认监听 `http://localhost:8081`，可用 `/api/auth/login` 或健康检查接口验证。
6. 测试：执行 `mvn test`。

## 配置要点与运行

- 端口：`server.port` 默认 8081。
- 数据源：`spring.datasource.*` 需与 MySQL 实例匹配；`schema.sql` 启动时自动建表，`ddl-auto: update` 会同步结构。
- MQTT：
	- `carebed.mqtt.enabled` 控制是否启用 MQTT 桥接。
	- `carebed.mqtt.bridge.host/port` 指向外部 Broker，用于与设备通信；`heartbeat-timeout-ms` 与 `offline-check-interval-ms` 控制心跳判定。
	- 默认主题：设备心跳 `devices/{deviceCode}/heartbeat`，指令 `devices/{deviceCode}/command`。
- 日志与监控：建议结合 Actuator 与外部日志收集（参见后文“运维与部署建议”）。

## 系统概览

- 支持陪护家属与运营管理员多角色登录与鉴权。
- 统一管理共享陪护床设备的注册、绑定、状态与故障。
- 覆盖租借、使用计费、归还、超时控制的全链路流程。
- 内置钱包账户体系与扣费、退款、充值、争议处理能力。
- 实时推送租借、支付、故障、维修等场景通知。
- 提供管理员综合看板，展示用户、设备、订单、财务指标。

---

## 架构与技术栈

| 领域 | 技术/方案 |
| ---- | -------- |
| 语言 | Java 21 |
| 框架 | Spring Boot 3.2.x、Spring MVC、Spring Validation、Spring Security (仅做简单 Token 校验) |
| 构建 | Maven |
## 未来规划

- **设备物联网化**：继续强化 MQTT/设备指令链路，增加设备端回执与更多指令类型。
- **消息多渠道**：集成短信、微信公众号、小程序模板消息等。
- **多医院支持**：引入组织维度的多租户设计，实现跨院区管理。
- **计费策略升级**：支持按次、按天、阶梯价、优惠券、押金等模式。
- **风险控制**：增加设备盗损检测、异常用电报警、人机巡检流程。
- **数据分析**：引入大屏可视化，实时展示床位分布、使用热力图、维修 SLA。

---

## 角色与权限矩阵

| 功能 / 角色 | 陪护家属 (FAMILY) | 运营管理员 (ADMIN) |
| ------------ | ----------------- | ------------------ |
| 注册 / 登录 | ✔ | ✔ |
| 关联患者信息 | ✔ | ✖ |
| 浏览设备列表 | 只读 | CRUD |
| 设备绑定/解绑 | 自身租借流程触发 | 手动干预 |
| 发起租借/归还 | ✔ | 可代操作 |
| 查看订单 | 仅本人 | 全量 |
| 钱包充值 / 查询 | ✔ | ✔（可查看所有用户申诉） |
| 发起费用争议 | ✔ | 审核处理 |
| 提交报修 | ✔ | ✔（可新建及修改状态） |
| 处理报修 | ✖ | ✔ |
| 查看通知 | ✔ | ✔（含管理员渠道） |
| 查看运营看板 | ✖ | ✔ |

---

## 功能模块详解

### 1. 用户与认证

- `POST /api/auth/register` 创建账号，支持 FAMILY/ADMIN 角色。
- `POST /api/auth/login` 登录后返回临时会话 Token（有效期 12 小时）。
- `POST /api/auth/link-patient` 家属可填写住院号/床位号，方便订单关联。
- 数据结构：`UserAccount`（UUID 主键、BCrypt 加密密码、角色、联系方式、患者关联信息）。

### 2. 智能设备管理与 MQTT

- 设备注册、更新、删除、绑定、释放、心跳上报、故障上报、远程重启。
- MQTT 桥接：订阅 `devices/{code}/heartbeat`；向 `devices/{code}/command` 以 QoS2 下发 UNLOCK/REBOOT（设备可用 QoS1 订阅以兼容 PubSubClient）。
- MQTT 收发日志持久化，可通过 `/api/mqtt/logs`（管理员）查看最近 200 条。
- 设备状态枚举：`AVAILABLE`、`IN_USE`、`MAINTENANCE`、`OFFLINE`；事件流 `DeviceEvent` 追踪生命周期。

### 3. 租借使用与归还

- `POST /api/rentals/start` 启动租借，绑定设备与用户。
- `POST /api/rentals/{id}/return` 归还设备并完成计费（按小时向上取整）。
- 支持管理员读取全部订单、家属仅查看自身订单。
- 内置超时扫描方法 `scanForOverdue()`，可通过定时任务触发。

### 4. 消息与通知

- 所有关键动作触发 Notification，例如租借成功、超时提醒、扣费通知、维修更新。
- 用户与管理员分别维护收件箱，可手动标记已读。

### 5. 报修与维修

- `POST /api/repairs` 家属发起报修，上传描述与照片（Base64 或外链）。
- 系统自动关联当前订单与设备，管理员可更新状态：`OPEN` → `IN_PROGRESS` → `RESOLVED`。

### 6. 我的钱包

- 钱包账户 `WalletAccount` 记录余额，所有交易写入 `WalletTransaction`。
- 支持充值、扣费、退款、争议处理；管理员可调整争议单状态及处理意见。

### 7. 平台综合管理

- 管理员概览 `GET /api/admin/overview` 展示用户、设备、订单、钱包余额、报修情况等统计。
- `GET /api/admin/analytics` 返回使用趋势与营收趋势，可用于可视化。

---

## 核心业务流程

### 租借流程（家属端）

1. 登录并关联患者信息。
2. 扫码选择设备，调用 `/api/rentals/start`。
3. 系统：
	 - 校验设备状态 → 将设备标记为 `IN_USE`。
	 - 创建 `RentalRecord`，记录预期结束时间（按期望时长）。
	 - 发送租借成功通知给家属和管理员。
4. 使用结束后，家属归还：`/api/rentals/{id}/return`，需确认关锁。
5. 系统：
	 - 计算使用时长与费用 → 钱包扣费。
	 - 释放设备，恢复 `AVAILABLE`。
	 - 发送关锁确认与扣费通知。

### 超时检测

- 定期执行 `RentalService.scanForOverdue()`：
	- 找出预期结束时间已过且仍 `ACTIVE` 的订单。
	- 将订单标记为 `OVERDUE`，推送“即将超时/已超时”提醒。

### 报修流程

1. 家属在订单中发现故障，提交报修。
2. 系统关联设备与订单，通知管理员。
3. 管理员更新状态及处理说明，用户实时接收进度通知。
4. 设备可在维修状态中被下架或安排维护。

### 钱包争议处理

1. 用户在钱包模块发起费用争议（指定订单号、原因）。
2. 管理员列表查看全部争议，更新状态为 `IN_REVIEW / RESOLVED / REJECTED`，填写结论。
3. 对应通知推送给用户，必要时可补发退款交易。

---

## 数据模型速览

| 实体 | 关键字段 |
| ---- | -------- |
| UserAccount | `id`、`username`、`passwordHash`、`role`、`linkedPatientId`、`createdAt` |
| Device | `id`、`deviceCode`、`ward`、`bedNumber`、`status`、`batteryLevel`、`lastHeartbeat` |
| RentalRecord | `id`、`userId`、`deviceId`、`status`、`startedAt`、`expectedEndAt`、`amount` |
| WalletAccount | `userId`、`balance`、`updatedAt` |
| WalletTransaction | `id`、`userId`、`type` (RECHARGE/DEBIT/REFUND/ADJUSTMENT)、`amount`、`orderId` |
| NotificationMessage | `id`、`userId`、`type`、`title`、`content`、`read`、`createdAt` |
| RepairTicket | `id`、`userId`、`rentalId`、`deviceId`、`description`、`photos`、`status` |

---

## API 参考

以下为关键接口示例（完整定义参见 `src/main/java/com/carebed/**/controller`）：

### 认证与用户

```http
POST /api/auth/register
Content-Type: application/json

{
	"username": "family001",
	"password": "Password@123",
	"role": "FAMILY",
	"fullName": "张三",
	"phone": "+8613811112222"
}
```

成功返回：`{"token":"...","role":"FAMILY","displayName":"张三"}`。

### 设备管理

```http
POST /api/devices
Content-Type: application/json

{
	"deviceCode": "BED-0001",
	"ward": "内科三区",
	"bedNumber": "305-2"
}
```

### 租借归还

```http
POST /api/rentals/{id}/return
X-Auth-Token: <登录返回的 token>
Content-Type: application/json

{
	"conditionNotes": "设备完好",
	"lockConfirmed": true
}
```

若余额不足扣费，接口返回 400 并提示“余额不足，请充值”。

### 钱包申诉

```http
POST /api/wallet/disputes
X-Auth-Token: <token>
Content-Type: application/json

{
	"orderId": "a0f1...",
	"reason": "扣费金额异常"
}
```

管理员处理争议：`POST /api/wallet/disputes/{id}`，更新状态与处理说明。

---

## 通知策略

| 触发事件 | 通知类型 | 接收方 | 说明 |
| -------- | -------- | ------ | ---- |
| 租借创建 | RENTAL_SUCCESS | 家属、管理员 | 提示订单开始 |
| 预期超时 | RENTAL_EXPIRING | 家属 | 每次扫描发现超时即发送 |
| 归还确认 | LOCK_CONFIRMED | 家属 | 记录归还完成 |
| 钱包扣费 | PAYMENT_CHARGED | 家属、管理员 | 通知扣费或退款金额 |
| 充值成功 | PAYMENT_ALERT | 家属 | 提醒余额变动 |
| 报修提交 | REPAIR_UPDATE | 管理员 | 鼓励及时处理 |
| 报修状态变化 | REPAIR_UPDATE | 家属 | 监控维修进度 |
| 系统异常/取消 | SYSTEM_ALERT | 相关用户 | 如订单被取消 |

通知存储在数据库表 `notification_messages`，可按需扩展到消息队列、短信、推送平台。

---

## 测试与质量保障

- 单元测试：`mvn test`
- 编译检查：`mvn -DskipTests compile`
- 建议引入以下实践：
	- 使用 Testcontainers / H2 模拟真实数据库。
	- 在租借、扣费等关键流程加入集成测试。
	- 接入 SonarQube 或 SpotBugs 做静态扫描。

---

## 运维与部署建议

1. **外部化配置**：使用 `application-prod.yml` 和环境变量，避免硬编码密码。
2. **日志管理**：集成 ELK/EFK 并对关键操作（扣费、故障）写入审计日志。
3. **持久化存储**：迁移至数据库后，为核心表加上乐观锁或悲观锁确保一致性。
4. **高可用**：
	 - 使用 Nginx/负载均衡进行流量分发。
	 - 结合 Spring Cloud 或 Kubernetes，实现弹性伸缩。
5. **监控告警**：借助 Actuator + Prometheus + Grafana 实现性能与业务指标监控。

---

## 常见问题

1. **为什么没有看到数据表或数据？**
	 - 检查 MySQL 连接是否正确；启动时会执行 `schema.sql` 自动建表。确认使用的数据库实例与应用配置一致。

2. **如何扩展 Token 机制？**
	 - 可替换为 JWT（Spring Security + jjwt），或改用 OAuth2/OpenID Connect。

3. **如何接入真实支付？**
	 - 当前钱包为虚拟账户，接入第三方支付需新增支付回调、账务对账表与签名校验逻辑。

4. **蓝牙开锁如何对接？**
	 - 在租借开始前调用第三方蓝牙服务 API，并将结果写入 `DeviceEvent`，与现有远程指令/事件流对齐。

---

