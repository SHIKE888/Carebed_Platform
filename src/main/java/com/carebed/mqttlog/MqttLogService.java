package com.carebed.mqttlog;

import org.springframework.data.domain.PageRequest;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.util.List;
import java.util.UUID;

@Service
public class MqttLogService {

    private static final int DEFAULT_LIMIT = 200;

    private final MqttLogRepository repository;

    public MqttLogService(MqttLogRepository repository) {
        this.repository = repository;
    }

    @Transactional
    public void recordIn(String topic, String payload, String deviceCode) {
        save("IN", topic, payload, deviceCode);
    }

    @Transactional
    public void recordOut(String topic, String payload, String deviceCode) {
        save("OUT", topic, payload, deviceCode);
    }

    @Transactional(readOnly = true)
    public List<MqttLogEntry> listLatest() {
        return repository.findAllByOrderByCreatedAtDesc(PageRequest.of(0, DEFAULT_LIMIT))
                .stream()
                .map(this::toEntry)
                .toList();
    }

    private void save(String direction, String topic, String payload, String deviceCode) {
        MqttLogEntity entity = new MqttLogEntity();
        entity.setId(UUID.randomUUID());
        entity.setDirection(direction);
        entity.setTopic(topic);
        entity.setPayload(payload);
        entity.setDeviceCode(deviceCode);
        entity.setCreatedAt(Instant.now());
        repository.save(entity);
    }

    private MqttLogEntry toEntry(MqttLogEntity entity) {
        return new MqttLogEntry(entity.getId(), entity.getDirection(), entity.getTopic(), entity.getPayload(),
                entity.getDeviceCode(), entity.getCreatedAt());
    }
}