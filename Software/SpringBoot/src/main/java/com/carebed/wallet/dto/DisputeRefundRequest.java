package com.carebed.wallet.dto;

import jakarta.validation.constraints.DecimalMin;
import jakarta.validation.constraints.NotNull;

import java.math.BigDecimal;

public record DisputeRefundRequest(
        @NotNull @DecimalMin(value = "0.01", message = "退款金额必须大于0") BigDecimal refundAmount) {
}
