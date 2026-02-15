package com.carebed.notification.persistence;

import com.carebed.notification.NotificationType;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;
import java.util.Optional;
import java.util.UUID;

public interface NotificationMessageRepository extends JpaRepository<NotificationMessageEntity, UUID> {

    List<NotificationMessageEntity> findByRecipientIdOrderByCreatedAtDesc(UUID recipientId);

    Optional<NotificationMessageEntity> findFirstByRecipientIdAndTypeOrderByCreatedAtDesc(UUID recipientId,
            NotificationType type);
}
