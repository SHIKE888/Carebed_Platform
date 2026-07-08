package com.carebed.device;

import com.carebed.common.model.OperationResult;
import com.carebed.device.dto.DeviceBindingRequest;
import com.carebed.device.dto.DeviceRegisterRequest;
import com.carebed.device.dto.DeviceResponse;
import com.carebed.device.dto.DeviceUpdateRequest;
import com.carebed.device.dto.FaultReportRequest;
import com.carebed.device.dto.HeartbeatUpdateRequest;
import com.carebed.device.dto.UnlockAllResponse;
import com.carebed.common.exception.BadRequestException;
import com.carebed.device.mqtt.DeviceCommandGateway;
import com.carebed.device.DeviceOnlineStatus;
import jakarta.validation.Valid;
import org.springframework.http.ResponseEntity;
import org.springframework.beans.factory.ObjectProvider;
import org.springframework.web.bind.annotation.DeleteMapping;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.PutMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import java.util.List;
import java.util.UUID;

@RestController
@RequestMapping("/api/devices")
public class DeviceController {

    private final DeviceService deviceService;
    private final DeviceCommandGateway deviceCommandGateway;

    public DeviceController(DeviceService deviceService, ObjectProvider<DeviceCommandGateway> gatewayProvider) {
        this.deviceService = deviceService;
        this.deviceCommandGateway = gatewayProvider.getIfAvailable();
    }

    @PostMapping
    public ResponseEntity<DeviceResponse> register(@Valid @RequestBody DeviceRegisterRequest request) {
        return ResponseEntity.ok(deviceService.register(request));
    }

    @PutMapping("/{id}")
    public ResponseEntity<DeviceResponse> update(@PathVariable UUID id,
            @Valid @RequestBody DeviceUpdateRequest request) {
        return ResponseEntity.ok(deviceService.update(id, request));
    }

    @GetMapping
    public ResponseEntity<List<DeviceResponse>> list(
            @RequestParam(name = "status", required = false) DeviceStatus status) {
        return ResponseEntity.ok(deviceService.list(status));
    }

    @GetMapping("/{id}")
    public ResponseEntity<DeviceResponse> detail(@PathVariable UUID id) {
        return ResponseEntity.ok(deviceService.detail(id));
    }

    @DeleteMapping("/{id}")
    public ResponseEntity<OperationResult> delete(@PathVariable UUID id) {
        return ResponseEntity.ok(deviceService.delete(id));
    }

    @PostMapping("/{id}/faults")
    public ResponseEntity<DeviceResponse> reportFault(@PathVariable UUID id,
            @Valid @RequestBody FaultReportRequest request) {
        return ResponseEntity.ok(deviceService.reportFault(id, request));
    }

    @PostMapping("/{id}/reboot")
    public ResponseEntity<OperationResult> reboot(@PathVariable UUID id) {
        if (deviceCommandGateway == null) {
            throw new BadRequestException("MQTT 功能未启用，无法下发重启指令");
        }
        return ResponseEntity.ok(deviceCommandGateway.sendRebootCommand(id));
    }

    @PostMapping("/{id}/bind")
    public ResponseEntity<DeviceResponse> bind(@PathVariable UUID id,
            @Valid @RequestBody DeviceBindingRequest request) {
        return ResponseEntity.ok(deviceService.bindDevice(id, request.patientReference()));
    }

    @PostMapping("/{id}/release")
    public ResponseEntity<DeviceResponse> release(@PathVariable UUID id) {
        return ResponseEntity.ok(deviceService.releaseDevice(id));
    }

    @PostMapping("/{id}/heartbeat")
    public ResponseEntity<DeviceResponse> heartbeat(@PathVariable UUID id,
            @Valid @RequestBody HeartbeatUpdateRequest request) {
        return ResponseEntity.ok(deviceService.refreshHeartbeat(id, request.batteryLevel(), request.lockStatus()));
    }

    @PostMapping("/{id}/unlock")
    public ResponseEntity<OperationResult> unlock(@PathVariable UUID id) {
        if (deviceCommandGateway == null) {
            throw new BadRequestException("MQTT 功能未启用，无法下发开锁指令");
        }
        return ResponseEntity.ok(deviceCommandGateway.sendUnlockCommand(id));
    }

    @PostMapping("/unlock-all")
    public ResponseEntity<UnlockAllResponse> unlockAllOnline() {
        if (deviceCommandGateway == null) {
            throw new BadRequestException("MQTT 功能未启用，无法下发开锁指令");
        }
        List<DeviceResponse> allDevices = deviceService.list(DeviceStatus.AVAILABLE);
        List<String> onlineCodes = allDevices.stream()
                .filter(d -> d.onlineStatus() == DeviceOnlineStatus.ONLINE)
                .map(DeviceResponse::deviceCode)
                .toList();
        int successCount = 0;
        List<String> failedCodes = new java.util.ArrayList<>();
        for (String deviceCode : onlineCodes) {
            try {
                var device = deviceService.findByCode(deviceCode);
                if (device.isPresent()) {
                    deviceCommandGateway.sendUnlockCommand(device.get().id());
                    successCount++;
                }
            } catch (Exception e) {
                failedCodes.add(deviceCode);
            }
        }
        return ResponseEntity.ok(new UnlockAllResponse(onlineCodes.size(), successCount, failedCodes));
    }
}
