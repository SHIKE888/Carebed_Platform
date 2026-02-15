package com.carebed.repair;

import java.time.Instant;
import java.util.List;
import java.util.UUID;

public record RepairTicket(
        UUID id,
        UUID userId,
        UUID rentalId,
        UUID deviceId,
        String deviceCode,
        String description,
        List<String> photos,
        RepairStatus status,
        String resolution,
        Instant createdAt,
        Instant updatedAt) {
    public RepairTicket withStatus(RepairStatus status, String resolution, Instant now) {
        return new RepairTicket(id, userId, rentalId, deviceId, deviceCode, description, photos, status, resolution,
                createdAt, now);
    }
}
