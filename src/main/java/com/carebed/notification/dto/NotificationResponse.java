package com.carebed.notification.dto;

import com.carebed.notification.NotificationType;

import java.time.Instant;

public record NotificationResponse(
        String id,
        NotificationType type,
        String title,
        String content,
        boolean read,
        Instant createdAt) {
}
