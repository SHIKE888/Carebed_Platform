package com.carebed.repair.dto;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;

import java.util.List;
import java.util.UUID;

public record RepairTicketCreateRequest(
        @NotNull(message = "订单ID不能为空") UUID rentalId,
        @NotBlank(message = "故障描述不能为空") String description,
        List<String> photos) {
}
