package com.carebed.notification;

import com.carebed.notification.dto.NotificationResponse;
import com.carebed.notification.persistence.NotificationMessageEntity;
import com.carebed.notification.persistence.NotificationMessageRepository;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.util.List;
import java.util.Optional;
import java.util.UUID;

@Service
public class NotificationService {

    private static final UUID ADMIN_CHANNEL = new UUID(0L, 0L);

    private final NotificationMessageRepository notificationMessageRepository;

    public NotificationService(NotificationMessageRepository notificationMessageRepository) {
        this.notificationMessageRepository = notificationMessageRepository;
    }

    @Transactional
    public NotificationMessage notifyUser(UUID userId, NotificationType type, String title, String content) {
        NotificationMessageEntity entity = new NotificationMessageEntity();
        entity.setId(UUID.randomUUID());
        entity.setRecipientId(userId);
        entity.setType(type);
        entity.setTitle(title);
        entity.setContent(content);
        entity.setRead(false);
        entity.setCreatedAt(Instant.now());
        return toDomain(notificationMessageRepository.save(entity));
    }

    public NotificationMessage notifyAdmins(NotificationType type, String title, String content) {
        return notifyUser(ADMIN_CHANNEL, type, title, content);
    }

    @Transactional(readOnly = true)
    public List<NotificationResponse> listUserMessages(UUID userId) {
        return notificationMessageRepository.findByRecipientIdOrderByCreatedAtDesc(userId).stream()
                .map(this::toDomain)
                .map(this::toResponse)
                .toList();
    }

    public List<NotificationResponse> listAdminMessages() {
        return listUserMessages(ADMIN_CHANNEL);
    }

    @Transactional
    public void markRead(UUID userId, UUID messageId) {
        notificationMessageRepository.findById(messageId).ifPresent(entity -> {
            if (!entity.getRecipientId().equals(userId)) {
                return;
            }
            entity.setRead(true);
            notificationMessageRepository.save(entity);
        });
    }

    @Transactional(readOnly = true)
    public Optional<NotificationMessage> latestMessage(UUID userId, NotificationType type) {
        return notificationMessageRepository
                .findFirstByRecipientIdAndTypeOrderByCreatedAtDesc(userId, type)
                .map(this::toDomain);
    }

    private NotificationResponse toResponse(NotificationMessage message) {
        return new NotificationResponse(
                message.id().toString(),
                message.type(),
                message.title(),
                message.content(),
                message.read(),
                message.createdAt());
    }

    private NotificationMessage toDomain(NotificationMessageEntity entity) {
        return new NotificationMessage(
                entity.getId(),
                entity.getRecipientId(),
                entity.getType(),
                entity.getTitle(),
                entity.getContent(),
                entity.isRead(),
                entity.getCreatedAt());
    }
}
