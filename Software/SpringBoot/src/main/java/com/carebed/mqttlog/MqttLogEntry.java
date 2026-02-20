package com.carebed.mqttlog;

import java.time.Instant;
import java.util.UUID;

public record MqttLogEntry(
        UUID id,
        String direction,
        String topic,
        String payload,
        String deviceCode,
        Instant createdAt) {
}