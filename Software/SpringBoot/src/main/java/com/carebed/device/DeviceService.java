package com.carebed.device;

import com.carebed.common.exception.BadRequestException;
import com.carebed.common.exception.ResourceNotFoundException;
import com.carebed.common.model.OperationResult;
import com.carebed.device.dto.DeviceRegisterRequest;
import com.carebed.device.dto.DeviceResponse;
import com.carebed.device.dto.DeviceUpdateRequest;
import com.carebed.device.dto.FaultReportRequest;
import com.carebed.device.persistence.DeviceEntity;
import com.carebed.device.persistence.DeviceEventEntity;
import com.carebed.device.persistence.DeviceEventRepository;
import com.carebed.device.persistence.DeviceRepository;
import jakarta.validation.constraints.NotNull;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Duration;
import java.time.Instant;
import java.util.List;
import java.util.Optional;
import java.util.UUID;

@Service
@Transactional
public class DeviceService {

    private final DeviceRepository deviceRepository;
    private final DeviceEventRepository deviceEventRepository;

    public DeviceService(DeviceRepository deviceRepository, DeviceEventRepository deviceEventRepository) {
        this.deviceRepository = deviceRepository;
        this.deviceEventRepository = deviceEventRepository;
    }

    public DeviceResponse register(DeviceRegisterRequest request) {
        boolean exists = deviceRepository.existsByDeviceCodeIgnoreCase(request.deviceCode());
        if (exists) {
            throw new BadRequestException("设备编号已存在");
        }
        Instant now = Instant.now();
        DeviceEntity entity = new DeviceEntity();
        entity.setId(UUID.randomUUID());
        entity.setDeviceCode(request.deviceCode());
        entity.setWard(request.ward());
        entity.setBedNumber(request.bedNumber());
        entity.setStatus(DeviceStatus.AVAILABLE);
        entity.setBatteryLevel(100);
        entity.setBoundPatientReference(null);
        entity.setOnlineStatus(DeviceOnlineStatus.OFFLINE);
        entity.setLockStatus(DeviceLockStatus.LOCKED);
        entity.setLastHeartbeat(null);
        entity.setCreatedAt(now);
        entity.setUpdatedAt(now);
        deviceRepository.save(entity);
        appendEvent(entity, now, "REGISTER", "设备入库");
        return toResponse(entity);
    }

    public DeviceResponse update(UUID id, DeviceUpdateRequest request) {
        DeviceEntity entity = getDeviceInternal(id);
        Instant now = Instant.now();
        entity.setWard(request.ward());
        entity.setBedNumber(request.bedNumber());
        entity.setStatus(request.status());
        if (request.batteryLevel() != null) {
            entity.setBatteryLevel(request.batteryLevel());
        }
        entity.setUpdatedAt(now);
        appendEvent(entity, now, "UPDATE", "设备信息更新");
        return toResponse(entity);
    }

    public OperationResult delete(UUID id) {
        DeviceEntity entity = getDeviceInternal(id);
        deviceEventRepository.deleteByDeviceId(id);
        deviceRepository.delete(entity);
        return OperationResult.of("设备已删除");
    }

    @Transactional(readOnly = true)
    public List<DeviceResponse> list(DeviceStatus status) {
        List<DeviceEntity> devices = deviceRepository.findAll();
        return devices.stream()
                .sorted((left, right) -> left.getCreatedAt().compareTo(right.getCreatedAt()))
                .filter(device -> status == null || presentableStatus(device) == status)
                .map(this::toResponse)
                .toList();
    }

    @Transactional(readOnly = true)
    public DeviceResponse detail(UUID id) {
        DeviceEntity entity = getDeviceInternal(id);
        return toResponse(entity);
    }

    @Transactional(readOnly = true)
    public Device getDevice(UUID id) {
        DeviceEntity entity = getDeviceInternal(id);
        return toDevice(entity);
    }

    @Transactional(readOnly = true)
    public List<Device> allDevices() {
        return deviceRepository.findAll().stream()
                .sorted((left, right) -> left.getCreatedAt().compareTo(right.getCreatedAt()))
                .map(this::toDevice)
                .toList();
    }

    public DeviceResponse reportFault(UUID id, FaultReportRequest request) {
        DeviceEntity entity = getDeviceInternal(id);
        Instant now = Instant.now();
        entity.setStatus(DeviceStatus.MAINTENANCE);
        entity.setUpdatedAt(now);
        appendEvent(entity, now, "FAULT", request.description());
        return toResponse(entity);
    }

    public DeviceResponse bindDevice(UUID id, String patientReference) {
        DeviceEntity entity = getDeviceInternal(id);
        if (entity.getStatus() == DeviceStatus.MAINTENANCE) {
            throw new BadRequestException("维护中的设备不可绑定");
        }
        Instant now = Instant.now();
        entity.setBoundPatientReference(patientReference);
        entity.setStatus(DeviceStatus.IN_USE);
        entity.setUpdatedAt(now);
        appendEvent(entity, now, "BIND", "绑定患者" + patientReference);
        return toResponse(entity);
    }

    public DeviceResponse releaseDevice(UUID id) {
        DeviceEntity entity = getDeviceInternal(id);
        Instant now = Instant.now();
        entity.setStatus(DeviceStatus.AVAILABLE);
        entity.setBoundPatientReference(null);
        entity.setUpdatedAt(now);
        appendEvent(entity, now, "RELEASE", "解除绑定并恢复可用");
        return toResponse(entity);
    }

    public DeviceResponse refreshHeartbeat(UUID id, @NotNull Integer batteryLevel, DeviceLockStatus lockStatus) {
        DeviceEntity entity = getDeviceInternal(id);
        return applyHeartbeat(entity, batteryLevel, lockStatus);
    }

    public DeviceResponse refreshHeartbeat(String deviceCode, int batteryLevel, DeviceLockStatus lockStatus) {
        DeviceEntity entity = deviceRepository.findByDeviceCodeIgnoreCase(deviceCode)
                .orElseThrow(() -> new ResourceNotFoundException("设备不存在: " + deviceCode));
        return applyHeartbeat(entity, batteryLevel, lockStatus);
    }

    @Transactional(readOnly = true)
    public Optional<Device> findByCode(String code) {
        return deviceRepository.findByDeviceCodeIgnoreCase(code).map(this::toDevice);
    }

    public void markOfflineDevices(Duration timeout) {
        Instant threshold = Instant.now().minus(timeout);
        List<DeviceEntity> onlineDevices = deviceRepository.findByOnlineStatus(DeviceOnlineStatus.ONLINE);
        for (DeviceEntity device : onlineDevices) {
            Instant last = device.getLastHeartbeat();
            if (last == null || last.isBefore(threshold)) {
                DeviceStatus newStatus = device.getStatus() == DeviceStatus.MAINTENANCE ? device.getStatus()
                        : DeviceStatus.OFFLINE;
                Instant now = Instant.now();
                device.setOnlineStatus(DeviceOnlineStatus.OFFLINE);
                device.setStatus(newStatus);
                device.setUpdatedAt(now);
                appendEvent(device, now, "OFFLINE", "设备超过心跳超时阈值");
            }
        }
    }

    public void recordRemoteCommand(UUID id, String description) {
        DeviceEntity entity = getDeviceInternal(id);
        Instant now = Instant.now();
        entity.setUpdatedAt(now);
        appendEvent(entity, now, "COMMAND", description);
    }

    private DeviceEntity getDeviceInternal(UUID id) {
        return deviceRepository.findById(id)
                .orElseThrow(() -> new ResourceNotFoundException("设备不存在"));
    }

    private void appendEvent(DeviceEntity device, Instant timestamp, String type, String description) {
        DeviceEventEntity event = new DeviceEventEntity();
        event.setDevice(device);
        event.setTimestamp(timestamp);
        event.setType(type);
        event.setDescription(description);
        deviceEventRepository.save(event);
    }

    private DeviceStatus resolveBusinessStatus(DeviceEntity device) {
        if (device.getStatus() == DeviceStatus.MAINTENANCE) {
            return DeviceStatus.MAINTENANCE;
        }
        if (device.getBoundPatientReference() != null && !device.getBoundPatientReference().isBlank()) {
            return DeviceStatus.IN_USE;
        }
        return DeviceStatus.AVAILABLE;
    }

    private DeviceStatus presentableStatus(DeviceEntity device) {
        if (device.getStatus() == DeviceStatus.MAINTENANCE) {
            return DeviceStatus.MAINTENANCE;
        }
        if (device.getOnlineStatus() == DeviceOnlineStatus.OFFLINE) {
            return DeviceStatus.OFFLINE;
        }
        if (device.getBoundPatientReference() != null && !device.getBoundPatientReference().isBlank()) {
            return DeviceStatus.IN_USE;
        }
        if (device.getStatus() == DeviceStatus.IN_USE) {
            return DeviceStatus.IN_USE;
        }
        return DeviceStatus.AVAILABLE;
    }

    private DeviceResponse applyHeartbeat(DeviceEntity entity, int batteryLevel, DeviceLockStatus lockStatus) {
        Instant now = Instant.now();
        entity.setBatteryLevel(batteryLevel);
        entity.setLockStatus(lockStatus);
        entity.setOnlineStatus(DeviceOnlineStatus.ONLINE);
        entity.setLastHeartbeat(now);
        entity.setUpdatedAt(now);
        entity.setStatus(resolveBusinessStatus(entity));
        appendEvent(entity, now, "HEARTBEAT", "电量" + batteryLevel + "% 锁状态" + lockStatus.name());
        return toResponse(entity);
    }

    private Device toDevice(DeviceEntity entity) {
        DeviceStatus effectiveStatus = presentableStatus(entity);
        return new Device(
                entity.getId(),
                entity.getDeviceCode(),
                entity.getWard(),
                entity.getBedNumber(),
                effectiveStatus,
                entity.getBatteryLevel(),
                entity.getBoundPatientReference(),
                entity.getOnlineStatus(),
                entity.getLockStatus(),
                entity.getLastHeartbeat(),
                entity.getCreatedAt(),
                entity.getUpdatedAt());
    }

    private DeviceResponse toResponse(DeviceEntity entity) {
        List<DeviceEvent> events = deviceEventRepository.findByDevice_IdOrderByTimestampAsc(entity.getId()).stream()
                .map(event -> new DeviceEvent(event.getTimestamp(), event.getType(), event.getDescription()))
                .toList();
        Device device = toDevice(entity);
        return new DeviceResponse(
                device.id().toString(),
                device.deviceCode(),
                device.ward(),
                device.bedNumber(),
                device.status(),
                device.batteryLevel(),
                device.boundPatientReference(),
                device.onlineStatus(),
                device.lockStatus(),
                device.lastHeartbeat(),
                device.createdAt(),
                device.updatedAt(),
                events);
    }
}
