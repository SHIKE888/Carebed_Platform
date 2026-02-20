package com.carebed.mqttlog;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.Id;
import jakarta.persistence.Index;
import jakarta.persistence.Table;

import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "mqtt_logs", indexes = {
        @Index(name = "idx_mqtt_topic", columnList = "topic"),
        @Index(name = "idx_mqtt_device_code", columnList = "device_code")
})
public class MqttLogEntity {

    @Id
    @Column(nullable = false, updatable = false, columnDefinition = "BINARY(16)")
    private UUID id;

    @Column(nullable = false, length = 8)
    private String direction;

    @Column(nullable = false, length = 128)
    private String topic;

    @Column(nullable = false, length = 1024)
    private String payload;

    @Column(name = "device_code", length = 64)
    private String deviceCode;

    @Column(nullable = false)
    private Instant createdAt;

    public UUID getId() {
        return id;
    }

    public void setId(UUID id) {
        this.id = id;
    }

    public String getDirection() {
        return direction;
    }

    public void setDirection(String direction) {
        this.direction = direction;
    }

    public String getTopic() {
        return topic;
    }

    public void setTopic(String topic) {
        this.topic = topic;
    }

    public String getPayload() {
        return payload;
    }

    public void setPayload(String payload) {
        this.payload = payload;
    }

    public String getDeviceCode() {
        return deviceCode;
    }

    public void setDeviceCode(String deviceCode) {
        this.deviceCode = deviceCode;
    }

    public Instant getCreatedAt() {
        return createdAt;
    }

    public void setCreatedAt(Instant createdAt) {
        this.createdAt = createdAt;
    }
}