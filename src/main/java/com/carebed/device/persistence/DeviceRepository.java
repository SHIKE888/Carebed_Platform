package com.carebed.device.persistence;

import com.carebed.device.DeviceOnlineStatus;
import com.carebed.device.DeviceStatus;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;
import java.util.Optional;
import java.util.UUID;

public interface DeviceRepository extends JpaRepository<DeviceEntity, UUID> {

    boolean existsByDeviceCodeIgnoreCase(String deviceCode);

    Optional<DeviceEntity> findByDeviceCodeIgnoreCase(String deviceCode);

    List<DeviceEntity> findByStatus(DeviceStatus status);

    List<DeviceEntity> findByOnlineStatus(DeviceOnlineStatus status);
}
