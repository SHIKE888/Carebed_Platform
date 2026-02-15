package com.carebed.device.dto;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Pattern;

public record DeviceRegisterRequest(
        @NotBlank(message = "设备编号不能为空") @Pattern(regexp = "^[A-Z0-9_-]{4,32}$", message = "设备编号格式不正确") String deviceCode,
        @NotBlank(message = "病区不能为空") String ward,
        @NotBlank(message = "床位号不能为空") String bedNumber) {
}
