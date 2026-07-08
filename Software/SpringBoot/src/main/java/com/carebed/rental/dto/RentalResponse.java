package com.carebed.rental.dto;

import com.carebed.rental.RentalStatus;

import java.time.Instant;

public record RentalResponse(
        String id,
        String userId,
        String deviceId,
        String deviceCode,
        RentalStatus status,
        Instant startedAt,
        Instant endedAt,
        Instant expectedEndAt,
        double amount,
        long durationMinutes,
        String patientReference,
        boolean disputeRaised,
        Instant createdAt) {
}
