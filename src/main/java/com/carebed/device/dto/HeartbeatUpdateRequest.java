package com.carebed.device.dto;

import com.carebed.device.DeviceLockStatus;
import jakarta.validation.constraints.Max;
import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotNull;

public record HeartbeatUpdateRequest(
        @NotNull(message = "电量不能为空") @Min(value = 0, message = "电量不能小于0") @Max(value = 100, message = "电量不能大于100") Integer batteryLevel,
        @NotNull(message = "锁状态不能为空") DeviceLockStatus lockStatus) {
}
