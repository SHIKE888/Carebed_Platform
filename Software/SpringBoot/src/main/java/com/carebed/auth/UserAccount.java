package com.carebed.auth;

import java.time.Instant;
import java.util.Optional;
import java.util.UUID;

public record UserAccount(
        UUID id,
        String username,
        String passwordHash,
        UserRole role,
        String fullName,
        String phone,
        String linkedPatientId,
        Instant createdAt,
        Instant updatedAt) {
    public Optional<String> linkedPatientIdOpt() {
        return Optional.ofNullable(linkedPatientId);
    }

    public UserAccount withLinkedPatient(String patientId, Instant now) {
        return new UserAccount(id, username, passwordHash, role, fullName, phone, patientId, createdAt, now);
    }

    public UserAccount touch(Instant now) {
        return new UserAccount(id, username, passwordHash, role, fullName, phone, linkedPatientId, createdAt, now);
    }
}
