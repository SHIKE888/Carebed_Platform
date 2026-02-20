package com.carebed.device.mqtt;

import com.carebed.common.model.OperationResult;

import java.util.UUID;

public interface DeviceCommandGateway {
    OperationResult sendUnlockCommand(UUID deviceId);

    OperationResult sendRebootCommand(UUID deviceId);
}
