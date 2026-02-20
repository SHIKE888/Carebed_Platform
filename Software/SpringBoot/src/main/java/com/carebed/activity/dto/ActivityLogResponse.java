package com.carebed.activity.dto;

import java.time.Instant;

public record ActivityLogResponse(
        String id,
        String actorName,
        String action,
        String details,
        Instant createdAt) {
}
