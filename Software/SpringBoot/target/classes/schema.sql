CREATE TABLE IF NOT EXISTS notification_messages (
    id BINARY(16) NOT NULL,
    recipient_id BINARY(16) NOT NULL,
    type VARCHAR(32) NOT NULL,
    title VARCHAR(128) NOT NULL,
    content VARCHAR(1024) NOT NULL,
    is_read BIT(1) NOT NULL,
    created_at DATETIME(6) NOT NULL,
    PRIMARY KEY (id),
    KEY idx_notification_recipient (recipient_id),
    KEY idx_notification_type (type)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS mqtt_logs (
    id BINARY(16) NOT NULL,
    direction VARCHAR(8) NOT NULL,
    topic VARCHAR(128) NOT NULL,
    payload VARCHAR(1024) NOT NULL,
    device_code VARCHAR(64),
    created_at DATETIME(6) NOT NULL,
    PRIMARY KEY (id),
    KEY idx_mqtt_topic (topic),
    KEY idx_mqtt_device_code (device_code)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
