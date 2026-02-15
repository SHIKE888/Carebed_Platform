package com.carebed.device.persistence;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Modifying;
import org.springframework.data.jpa.repository.Query;

import java.util.List;
import java.util.UUID;

public interface DeviceEventRepository extends JpaRepository<DeviceEventEntity, Long> {

    List<DeviceEventEntity> findByDevice_IdOrderByTimestampAsc(UUID deviceId);

    @Modifying
    @Query("delete from DeviceEventEntity e where e.device.id = :deviceId")
    void deleteByDeviceId(UUID deviceId);
}
