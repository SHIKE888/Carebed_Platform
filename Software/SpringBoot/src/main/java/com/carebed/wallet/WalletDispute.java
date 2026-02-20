package com.carebed.wallet;

import java.time.Instant;
import java.util.UUID;

public record WalletDispute(
        UUID id,
        UUID userId,
        String orderId,
        String reason,
        DisputeStatus status,
        String resolution,
        Instant createdAt,
        Instant updatedAt) {
    public WalletDispute withStatus(DisputeStatus status, String resolution, Instant now) {
        return new WalletDispute(id, userId, orderId, reason, status, resolution, createdAt, now);
    }
}
