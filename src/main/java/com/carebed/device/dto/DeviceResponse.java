package com.carebed.device.dto;

import com.carebed.device.DeviceEvent;
import com.carebed.device.DeviceLockStatus;
import com.carebed.device.DeviceOnlineStatus;
import com.carebed.device.DeviceStatus;

import java.time.Instant;
import java.util.List;

public record DeviceResponse(
        String id,
        String deviceCode,
        String ward,
        String bedNumber,
        DeviceStatus status,
        Integer batteryLevel,
        String boundPatientReference,
        DeviceOnlineStatus onlineStatus,
        DeviceLockStatus lockStatus,
        Instant lastHeartbeat,
        Instant createdAt,
        Instant updatedAt,
        List<DeviceEvent> events) {
}
