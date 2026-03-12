# MySQL 持久化切换说明

## 1. 功能概述
- 原本依赖内存 Map 的设备数据与事件现已改为通过 Spring Data JPA 持久化到 MySQL。
- 新增实体：`devices`、`device_events` 表分别保存设备状态与事件时间线，重启后数据不再丢失。
- MQTT 心跳、开锁指令、管理员操作等事件会同步写入数据库，便于审计与后续 BI 分析。

## 2. MySQL 环境准备
```sql
-- 创建数据库与专用账号
CREATE DATABASE carebed DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'carebed'@'%' IDENTIFIED BY 'changeme';
GRANT ALL PRIVILEGES ON carebed.* TO 'carebed'@'%';
FLUSH PRIVILEGES;
```
> 建议根据生产安全要求修改默认密码，并限制允许访问的主机范围。

## 3. Spring Boot 配置
- 默认配置位于 [Software/SpringBoot/src/main/resources/application.yml](../Software/SpringBoot/src/main/resources/application.yml)：
  ```yaml
  spring:
    datasource:
      url: jdbc:mysql://localhost:3306/carebed?useSSL=false&serverTimezone=UTC&characterEncoding=UTF-8
      username: carebed
      password: changeme
      driver-class-name: com.mysql.cj.jdbc.Driver
    jpa:
      hibernate:
        ddl-auto: update
      properties:
        hibernate:
          format_sql: true
          dialect: org.hibernate.dialect.MySQLDialect
      open-in-view: false
  ```
- 生产环境推荐：
  1. 通过环境变量或外部 `application-prod.yml` 覆盖数据库凭证；
  2. 将 `ddl-auto` 调整为 `validate` 或由 Flyway/Liquibase 管理 schema；
  3. 若数据库与应用不在同一主机，请将 `localhost` 替换成实际数据库地址。

## 4. 表结构说明
- `devices`
  - 主键：`id` (UUID, BINARY(16))
  - 关键字段：`device_code`、`status`、`online_status`、`lock_status`、`last_heartbeat`、`created_at`、`updated_at`
- `device_events`
  - 主键：自增 ID
  - 外键：`device_id` (指向 `devices.id`)
  - 内容：`timestamp`、`type`、`description`

Hibernate 默认会自动建表；若需手工建表，可使用以下示例（仅供参考，具体以 Hibernate 生成的结构为准）：
```sql
CREATE TABLE devices (
  id BINARY(16) NOT NULL,
  device_code VARCHAR(64) NOT NULL UNIQUE,
  ward VARCHAR(128) NOT NULL,
  bed_number VARCHAR(64) NOT NULL,
  status VARCHAR(24) NOT NULL,
  battery_level INT,
  bound_patient_reference VARCHAR(128),
  online_status VARCHAR(16) NOT NULL,
  lock_status VARCHAR(16) NOT NULL,
  last_heartbeat DATETIME(6),
  created_at DATETIME(6) NOT NULL,
  updated_at DATETIME(6) NOT NULL,
  PRIMARY KEY (id)
) ENGINE=InnoDB;

CREATE TABLE device_events (
  id BIGINT NOT NULL AUTO_INCREMENT,
  device_id BINARY(16) NOT NULL,
  timestamp DATETIME(6) NOT NULL,
  type VARCHAR(64) NOT NULL,
  description VARCHAR(512),
  PRIMARY KEY (id),
  KEY idx_device_events_device_id (device_id),
  CONSTRAINT fk_device_events_device FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
) ENGINE=InnoDB;
```

## 5. 部署/升级步骤
1. **备份**：若已有运行实例，请导出旧版数据（若此前使用的是内存存储则无需）。
2. **创建库与账号**：参照第 2 节初始化数据库。
3. **更新配置**：将生产环境的 Spring Boot 配置指向新的 MySQL 数据库。
4. **上线新版**：部署最新打包产物（`mvn clean package -DskipTests`）。
5. **验证**：
   - 启动后检查日志是否出现 Hibernate DDL 成功信息；
   - 通过 `mysql`/`Navicat` 查看 `devices`、`device_events` 是否生成并有数据；
   - 触发设备注册、心跳、开锁指令，确认记录写入。

## 6. 注意事项
- 新实现启用了 Spring Data JPA，应用启动时会检测数据库连接；若失败，服务将直接启动失败，请确保数据库可用。
- `device_events` 表数据可能迅速增长，建议：
  - 定期归档或清理历史数据；
  - 根据需求为 `timestamp` 添加索引以优化查询。
- 如需读写分离或连接池自定义，可在 `spring.datasource` 下配置 HikariCP 参数，如 `maximum-pool-size`。

完成以上配置后，后端会将设备状态和事件信息持久化到 MySQL，实现服务重启后的数据保留，并为后续数据分析提供基础。