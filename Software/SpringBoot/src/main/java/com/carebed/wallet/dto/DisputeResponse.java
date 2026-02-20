package com.carebed.wallet.dto;

import com.carebed.wallet.DisputeStatus;

import java.time.Instant;

public record DisputeResponse(
        String id,
        String userId,
        String orderId,
        String reason,
        DisputeStatus status,
        String resolution,
        Instant createdAt,
        Instant updatedAt) {
}
