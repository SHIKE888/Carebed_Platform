package com.carebed.auth.persistence;

import org.springframework.data.jpa.repository.JpaRepository;

import java.time.Instant;
import java.util.Optional;

public interface UserSessionRepository extends JpaRepository<UserSessionEntity, String> {

    Optional<UserSessionEntity> findByToken(String token);

    void deleteByExpiresAtBefore(Instant cutoff);
}
