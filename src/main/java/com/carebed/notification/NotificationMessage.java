package com.carebed.notification;

import java.time.Instant;
import java.util.UUID;

public record NotificationMessage(
        UUID id,
        UUID userId,
        NotificationType type,
        String title,
        String content,
        boolean read,
        Instant createdAt) {
    public NotificationMessage markRead() {
        return new NotificationMessage(id, userId, type, title, content, true, createdAt);
    }
}
