package com.carebed.wallet.dto;

import com.carebed.wallet.DisputeStatus;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;

public record DisputeUpdateRequest(
        @NotNull(message = "状态不能为空") DisputeStatus status,
        @NotBlank(message = "处理说明不能为空") String resolution) {
}
