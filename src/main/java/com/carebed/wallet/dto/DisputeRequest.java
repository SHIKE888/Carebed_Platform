package com.carebed.wallet.dto;

import jakarta.validation.constraints.NotBlank;

public record DisputeRequest(
        @NotBlank(message = "订单号不能为空") String orderId,
        @NotBlank(message = "申诉原因不能为空") String reason) {
}
