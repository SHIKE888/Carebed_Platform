package com.carebed.activity;

import java.time.Instant;
import java.util.UUID;

public record ActivityLogEntry(
        UUID id,
        UUID actorId,
        String actorName,
        String action,
        String details,
        Instant createdAt) {
}
