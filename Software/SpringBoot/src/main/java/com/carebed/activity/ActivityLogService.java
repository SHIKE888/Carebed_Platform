package com.carebed.activity;

import com.carebed.activity.dto.ActivityLogResponse;
import com.carebed.auth.UserAccount;
import org.springframework.data.domain.PageRequest;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.util.Collections;
import java.util.List;
import java.util.UUID;

@Service
public class ActivityLogService {

    private static final int DEFAULT_LIMIT = 30;

    private final ActivityLogRepository activityLogRepository;

    public ActivityLogService(ActivityLogRepository activityLogRepository) {
        this.activityLogRepository = activityLogRepository;
    }

    @Transactional
    public ActivityLogEntry record(UUID actorId, String actorName, String action, String details) {
        ActivityLogEntity entity = new ActivityLogEntity();
        entity.setId(UUID.randomUUID());
        entity.setActorId(actorId);
        entity.setActorName(actorName);
        entity.setAction(action);
        entity.setDetails(details);
        entity.setCreatedAt(Instant.now());
        ActivityLogEntity saved = activityLogRepository.save(entity);
        return toEntry(saved);
    }

    @Transactional(readOnly = true)
    public List<ActivityLogResponse> listFor(UserAccount requester) {
        boolean isAdmin = requester.role() == com.carebed.auth.UserRole.ADMIN;
        List<ActivityLogEntity> entities = isAdmin
                ? activityLogRepository.findAllByOrderByCreatedAtDesc(PageRequest.of(0, DEFAULT_LIMIT))
                : activityLogRepository.findByActorIdOrderByCreatedAtDesc(requester.id(),
                        PageRequest.of(0, DEFAULT_LIMIT));

        // Keep latest 200 while returning them in chronological order to avoid UI
        // flicker
        Collections.reverse(entities);
        return entities.stream()
                .map(this::toEntry)
                .map(this::toResponse)
                .toList();
    }

    private ActivityLogEntry toEntry(ActivityLogEntity entity) {
        return new ActivityLogEntry(
                entity.getId(),
                entity.getActorId(),
                entity.getActorName(),
                entity.getAction(),
                entity.getDetails(),
                entity.getCreatedAt());
    }

    private ActivityLogResponse toResponse(ActivityLogEntry entry) {
        return new ActivityLogResponse(
                entry.id().toString(),
                entry.actorName(),
                entry.action(),
                entry.details(),
                entry.createdAt());
    }
}
