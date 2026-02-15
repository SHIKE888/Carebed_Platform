# 生产部署指引（msas.absozero.cn）

## 一、前提条件
- 已获取一台可访问公网的 Linux 服务器（示例采用 Ubuntu 22.04）。
- 服务器域名：`msas.absozero.cn`，DNS 已解析到服务器公网 IP。
- 安装 JDK 21（推荐使用 Eclipse Temurin 或 OpenJDK）。
- 开放以下端口：
  - 8081：Spring Boot HTTP 服务
  - 1883：MQTT TCP 服务（ESP32 直连）
  - 22 / 80 / 443：按需开放用于 SSH、HTTP、HTTPS

## 二、构建后端应用
1. 在本地执行 Maven 打包：
   ```bash
   mvn clean package -DskipTests
   ```
2. 得到可执行包 `target/carebed-platform-0.0.1-SNAPSHOT.jar`。
3. 使用 `scp` 或其他方式上传到服务器目录（示例：`/opt/carebed`）。

## 三、准备数据库与配置文件
1. 创建 MySQL 数据库及账号（示例）：
   ```sql
   CREATE DATABASE carebed DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
   CREATE USER 'carebed'@'localhost' IDENTIFIED BY 'changeme';
   GRANT ALL PRIVILEGES ON carebed.* TO 'carebed'@'localhost';
   FLUSH PRIVILEGES;
   ```
   > 根据安全需求调整密码和访问来源；如数据库不与应用同机，请替换连接地址。

2. 在服务器 `/opt/carebed/config/application.yml` 内放置生产配置：
```yaml
server:
  port: 8081
spring:
  application:
    name: carebed-platform
      datasource:
        url: jdbc:mysql://127.0.0.1:3306/carebed?useSSL=false&serverTimezone=UTC&characterEncoding=UTF-8
        username: carebed
        password: changeme
        driver-class-name: com.mysql.cj.jdbc.Driver
      jpa:
        hibernate:
          ddl-auto: update
        properties:
          hibernate:
            dialect: org.hibernate.dialect.MySQLDialect
            format_sql: true
        open-in-view: false
carebed:
  mqtt:
    enabled: true
    broker:
      host: 0.0.0.0      # 嵌入式 broker 监听地址
      port: 1883
      persistence-path: /opt/carebed/mqtt-data
    bridge:
      host: localhost    # 内部桥接直连本机 broker
      port: 1883
      qos: 1
    heartbeat-timeout-ms: 90000
    offline-check-interval-ms: 30000
```
> 说明：
> - `broker.host` 设为 `0.0.0.0`，允许外部设备通过 `msas.absozero.cn:1883` 接入。
> - `persistence-path` 建议指向持久磁盘目录，避免重启后会话丢失。
> - 若需启用 MQTT 账号密码，可在 `MqttProperties` 中扩展鉴权逻辑，再在配置中填入 `broker.username`、`broker.password`。

## 四、创建运行脚本与 systemd 服务
1. 在 `/opt/carebed` 新建 `run.sh`：
   ```bash
   #!/usr/bin/env bash
   APP_HOME=/opt/carebed
   JAVA_OPTS="-Xms256m -Xmx512m"
   CONFIG="$APP_HOME/config/application.yml"
   exec java $JAVA_OPTS \
     -jar "$APP_HOME/carebed-platform-0.0.1-SNAPSHOT.jar" \
     --spring.config.location="file:$CONFIG"
   ```
   并赋予执行权限：`chmod +x /opt/carebed/run.sh`
2. 创建 systemd 单元 `/etc/systemd/system/carebed.service`：
   ```ini
   [Unit]
   Description=Carebed Platform Service
   After=network.target

   [Service]
   Type=simple
   WorkingDirectory=/opt/carebed
   ExecStart=/opt/carebed/run.sh
   Restart=on-failure
   User=carebed
   Group=carebed

   [Install]
   WantedBy=multi-user.target
   ```
3. 建议为服务创建专用账号：
   ```bash
   sudo useradd --system --home /opt/carebed --shell /usr/sbin/nologin carebed
   sudo chown -R carebed:carebed /opt/carebed
   ```
4. 重新加载 systemd 并启动：
   ```bash
   sudo systemctl daemon-reload
   sudo systemctl enable carebed
   sudo systemctl start carebed
   sudo systemctl status carebed
   ```

## 五、数据库与 MQTT 验证
1. 防火墙放行 1883 端口（以 UFW 为例）：
   ```bash
   sudo ufw allow 8081/tcp
   sudo ufw allow 1883/tcp
   sudo ufw reload
   ```
2. 确认应用成功连接 MySQL：
  - 查看 `journalctl -u carebed -f`，确保无 `DataSource` 或 Hibernate 错误；
  - 登录数据库检查 `devices`、`device_events` 表是否生成并写入记录。
3. 使用 MQTT 客户端（如 `mosquitto_sub`）测试：
   ```bash
   mosquitto_sub -h msas.absozero.cn -p 1883 -t "devices/+/heartbeat" -v
   ```
4. 让 ESP32 固件中的 `MQTT_HOST` 指向 `msas.absozero.cn`，端口保持 1883。
5. 若需启用 TLS：
   - 获取证书（例如使用 Let’s Encrypt）后配置 Nginx / HAProxy 监听 8883 加密端口。
   - 将加密代理转发至本地 1883（或使用 EMQX/Eclipse Mosquitto 等外部 Broker）。

## 六、HTTP 反向代理与 HTTPS（可选）
1. 安装 Nginx，并申请域名证书（可用 certbot）。
2. 在 `/etc/nginx/conf.d/carebed.conf` 中配置反向代理：
   ```nginx
   server {
       listen 80;
       server_name msas.absozero.cn;
       return 301 https://$host$request_uri;
   }

   server {
       listen 443 ssl;
       server_name msas.absozero.cn;

       ssl_certificate /etc/letsencrypt/live/msas.absozero.cn/fullchain.pem;
       ssl_certificate_key /etc/letsencrypt/live/msas.absozero.cn/privkey.pem;

       location / {
           proxy_pass http://127.0.0.1:8081;
           proxy_set_header Host $host;
           proxy_set_header X-Real-IP $remote_addr;
           proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
           proxy_set_header X-Forwarded-Proto $scheme;
       }
   }
   ```
3. 重新加载 Nginx：`sudo nginx -s reload`

## 七、日志与监控
- 应用日志默认输出到 systemd 日志，可通过 `journalctl -u carebed -f` 查看。
- 嵌入式 MQTT 目前默认允许匿名接入。若设备数量较多或需要外部监控，建议：
  - 引入专业 Broker（EMQX、Mosquitto 等）并将 `carebed.mqtt.enabled` 置为 `false`，让后端以客户端方式接入。
  - 或在现有嵌入式 Broker 上追加 ACL、连接数监控以及磁盘告警。

## 八、故障排查
- 服务无法启动：检查 `journalctl -u carebed`。
- ESP32 无法连接：确认 1883 端口放行、域名解析生效，必要时用 `telnet msas.absozero.cn 1883` 测试连通性。
- 心跳无更新：
  - 查看 `mosquitto_sub` 是否能收到心跳。
  - 查看后端日志是否出现解析失败或离线判定。
- 数据未写入 MySQL：检查数据库连通性、账号权限，以及 `spring.jpa.hibernate.ddl-auto` 是否设置为 `update`（或手工建表）。

完成上述步骤后，后端即可于 `https://msas.absozero.cn` 提供 Web API，设备通过 `msas.absozero.cn:1883` 使用 MQTT 与后端双向通信。