package com.carebed.activity;

import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;
import java.util.UUID;

public interface ActivityLogRepository extends JpaRepository<ActivityLogEntity, UUID> {

    List<ActivityLogEntity> findByActorIdOrderByCreatedAtDesc(UUID actorId, Pageable pageable);

    List<ActivityLogEntity> findAllByOrderByCreatedAtDesc(Pageable pageable);
}
