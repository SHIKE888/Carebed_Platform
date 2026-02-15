package com.carebed.device.dto;

import jakarta.validation.constraints.NotBlank;

public record DeviceBindingRequest(
        @NotBlank(message = "患者编号不能为空") String patientReference) {
}
