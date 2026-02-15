package com.carebed.device.dto;

import jakarta.validation.constraints.NotBlank;

public record FaultReportRequest(
        @NotBlank(message = "故障描述不能为空") String description) {
}
