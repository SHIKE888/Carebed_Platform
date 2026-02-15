package com.carebed.device.dto;

import com.carebed.device.DeviceStatus;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;

public record DeviceUpdateRequest(
        @NotBlank(message = "病区不能为空") String ward,
        @NotBlank(message = "床位号不能为空") String bedNumber,
        @NotNull(message = "设备状态不能为空") DeviceStatus status,
        Integer batteryLevel) {
}
