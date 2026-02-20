package com.carebed.device.persistence;

import com.carebed.device.DeviceLockStatus;
import com.carebed.device.DeviceOnlineStatus;
import com.carebed.device.DeviceStatus;
import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.EnumType;
import jakarta.persistence.Enumerated;
import jakarta.persistence.Id;
import jakarta.persistence.Table;

import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "devices")
public class DeviceEntity {

    @Id
    @Column(name = "id", nullable = false, columnDefinition = "BINARY(16)")
    private UUID id;

    @Column(name = "device_code", nullable = false, unique = true, length = 64)
    private String deviceCode;

    @Column(name = "ward", nullable = false, length = 128)
    private String ward;

    @Column(name = "bed_number", nullable = false, length = 64)
    private String bedNumber;

    @Enumerated(EnumType.STRING)
    @Column(name = "status", nullable = false, length = 24)
    private DeviceStatus status;

    @Column(name = "battery_level")
    private Integer batteryLevel;

    @Column(name = "bound_patient_reference", length = 128)
    private String boundPatientReference;

    @Enumerated(EnumType.STRING)
    @Column(name = "online_status", nullable = false, length = 16)
    private DeviceOnlineStatus onlineStatus;

    @Enumerated(EnumType.STRING)
    @Column(name = "lock_status", nullable = false, length = 16)
    private DeviceLockStatus lockStatus;

    @Column(name = "last_heartbeat")
    private Instant lastHeartbeat;

    @Column(name = "created_at", nullable = false)
    private Instant createdAt;

    @Column(name = "updated_at", nullable = false)
    private Instant updatedAt;

    public UUID getId() {
        return id;
    }

    public void setId(UUID id) {
        this.id = id;
    }

    public String getDeviceCode() {
        return deviceCode;
    }

    public void setDeviceCode(String deviceCode) {
        this.deviceCode = deviceCode;
    }

    public String getWard() {
        return ward;
    }

    public void setWard(String ward) {
        this.ward = ward;
    }

    public String getBedNumber() {
        return bedNumber;
    }

    public void setBedNumber(String bedNumber) {
        this.bedNumber = bedNumber;
    }

    public DeviceStatus getStatus() {
        return status;
    }

    public void setStatus(DeviceStatus status) {
        this.status = status;
    }

    public Integer getBatteryLevel() {
        return batteryLevel;
    }

    public void setBatteryLevel(Integer batteryLevel) {
        this.batteryLevel = batteryLevel;
    }

    public String getBoundPatientReference() {
        return boundPatientReference;
    }

    public void setBoundPatientReference(String boundPatientReference) {
        this.boundPatientReference = boundPatientReference;
    }

    public DeviceOnlineStatus getOnlineStatus() {
        return onlineStatus;
    }

    public void setOnlineStatus(DeviceOnlineStatus onlineStatus) {
        this.onlineStatus = onlineStatus;
    }

    public DeviceLockStatus getLockStatus() {
        return lockStatus;
    }

    public void setLockStatus(DeviceLockStatus lockStatus) {
        this.lockStatus = lockStatus;
    }

    public Instant getLastHeartbeat() {
        return lastHeartbeat;
    }

    public void setLastHeartbeat(Instant lastHeartbeat) {
        this.lastHeartbeat = lastHeartbeat;
    }

    public Instant getCreatedAt() {
        return createdAt;
    }

    public void setCreatedAt(Instant createdAt) {
        this.createdAt = createdAt;
    }

    public Instant getUpdatedAt() {
        return updatedAt;
    }

    public void setUpdatedAt(Instant updatedAt) {
        this.updatedAt = updatedAt;
    }
}
