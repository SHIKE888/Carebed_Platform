package com.carebed.rental;

import java.time.Duration;
import java.time.Instant;
import java.util.Optional;
import java.util.UUID;

public record RentalRecord(
        UUID id,
        UUID userId,
        UUID deviceId,
        String deviceCode,
        Instant startedAt,
        Instant endedAt,
        Instant expectedEndAt,
        RentalStatus status,
        double amount,
        String patientReference,
        boolean disputeRaised,
        Instant createdAt,
        Instant updatedAt) {
    public RentalRecord markCompleted(Instant endedAt, double amount) {
        return new RentalRecord(id, userId, deviceId, deviceCode, startedAt, endedAt, expectedEndAt,
                RentalStatus.COMPLETED, amount, patientReference, disputeRaised, createdAt, endedAt);
    }

    public RentalRecord markOverdue(Instant reference) {
        return new RentalRecord(id, userId, deviceId, deviceCode, startedAt, null, expectedEndAt, RentalStatus.OVERDUE,
                amount, patientReference, disputeRaised, createdAt, reference);
    }

    public RentalRecord raiseDispute() {
        return new RentalRecord(id, userId, deviceId, deviceCode, startedAt, endedAt, expectedEndAt, status, amount,
                patientReference, true, createdAt, updatedAt);
    }

    public Optional<Instant> getEndedAt() {
        return Optional.ofNullable(endedAt);
    }

    public Duration durationUntil(Instant reference) {
        Instant end = endedAt != null ? endedAt : reference;
        return Duration.between(startedAt, end);
    }
}
