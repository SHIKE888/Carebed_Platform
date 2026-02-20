package com.carebed.repair.dto;

import com.carebed.repair.RepairStatus;

import java.time.Instant;
import java.util.List;

public record RepairTicketResponse(
        String id,
        String userId,
        String rentalId,
        String deviceId,
        String deviceCode,
        String description,
        List<String> photos,
        RepairStatus status,
        String resolution,
        Instant createdAt,
        Instant updatedAt) {
}
