package com.carebed.repair.dto;

import com.carebed.repair.RepairStatus;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;

public record RepairTicketUpdateRequest(
        @NotNull(message = "处理状态不能为空") RepairStatus status,
        @NotBlank(message = "处理说明不能为空") String resolution) {
}
