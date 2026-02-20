package com.carebed.mqttlog;

import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;
import java.util.UUID;

public interface MqttLogRepository extends JpaRepository<MqttLogEntity, UUID> {

    List<MqttLogEntity> findAllByOrderByCreatedAtDesc(Pageable pageable);
}