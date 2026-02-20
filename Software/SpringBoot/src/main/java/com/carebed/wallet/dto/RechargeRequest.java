package com.carebed.wallet.dto;

import jakarta.validation.constraints.DecimalMin;
import jakarta.validation.constraints.Digits;
import jakarta.validation.constraints.NotNull;

import java.math.BigDecimal;

public record RechargeRequest(
        @NotNull(message = "充值金额不能为空") @DecimalMin(value = "0.01", message = "充值金额需大于0") @Digits(integer = 9, fraction = 2, message = "最多保留两位小数") BigDecimal amount) {
}
