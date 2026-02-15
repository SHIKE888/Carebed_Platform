package com.carebed.device;

import java.time.Instant;
import java.util.Optional;
import java.util.UUID;

public record Device(
        UUID id,
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
        Instant updatedAt) {
    public Optional<String> boundPatientReferenceOpt() {
        return Optional.ofNullable(boundPatientReference);
    }

    public Device withStatus(DeviceStatus newStatus, Instant now) {
        return new Device(id, deviceCode, ward, bedNumber, newStatus, batteryLevel, boundPatientReference,
                onlineStatus, lockStatus, lastHeartbeat, createdAt, now);
    }

    public Device withBinding(String reference, Instant now) {
        return new Device(id, deviceCode, ward, bedNumber, status, batteryLevel, reference, onlineStatus, lockStatus,
                lastHeartbeat, createdAt, now);
    }

    public Device refreshHeartbeat(int battery, DeviceLockStatus newLockStatus, DeviceStatus newStatus, Instant now) {
        return new Device(id, deviceCode, ward, bedNumber, newStatus, battery, boundPatientReference,
                DeviceOnlineStatus.ONLINE, newLockStatus, now, createdAt, now);
    }

    public Device markOffline(DeviceStatus newStatus, Instant now) {
        return new Device(id, deviceCode, ward, bedNumber, newStatus, batteryLevel, boundPatientReference,
                DeviceOnlineStatus.OFFLINE, lockStatus, lastHeartbeat, createdAt, now);
    }

    public Device updateLock(DeviceLockStatus newLockStatus, Instant now) {
        return new Device(id, deviceCode, ward, bedNumber, status, batteryLevel, boundPatientReference, onlineStatus,
                newLockStatus, lastHeartbeat, createdAt, now);
    }

    public Device touch(Instant now) {
        return new Device(id, deviceCode, ward, bedNumber, status, batteryLevel, boundPatientReference, onlineStatus,
                lockStatus, lastHeartbeat, createdAt, now);
    }

    public Device update(String ward, String bedNumber, DeviceStatus status, Integer battery, Instant now) {
        return new Device(id, deviceCode, ward, bedNumber, status, battery, boundPatientReference, onlineStatus,
                lockStatus, lastHeartbeat, createdAt, now);
    }
}
